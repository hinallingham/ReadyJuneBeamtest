#!/bin/bash

# ==============================================================================
#  MALTA2 Beam Center Check
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================

set -e

RUN_NUMBER=${1:-"001"}

CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"
GEOM_DIR="../../geometry/3malta_ref_60tilt"
GEOM_INIT="${GEOM_DIR}/3malta_init.conf"
GEOM_OUT="${GEOM_DIR}/3malta_beamcheck_masked.conf"

TMP_BEAMCHECK="tmp_beamcheck.conf"
BEAMCHECK_ROOT="output/beamcheck_run${RUN_NUMBER}.root"
OUTPUT_DIR="output/beamcheck"
OUTPUT_PNG="${OUTPUT_DIR}/beamcheck_run${RUN_NUMBER}.png"

CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_WARN="\e[1;33m"
CLR_RESET="\e[0m"

log_stage() { echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"; }
log_done()  { echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"; }
log_info()  { echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"; }

clear
echo -e "${CLR_INFO}=================================================${CLR_RESET}"
echo -e "  MALTA2 BEAM CENTER CHECK  --  Run ${RUN_NUMBER}          "
echo -e "${CLR_INFO}=================================================${CLR_RESET}"

mkdir -p "${OUTPUT_DIR}"

# ------------------------------------------------------------------------------
# Step 1: Run Corryvreckan (5000 events) + MaskCreator
# ------------------------------------------------------------------------------
log_stage "Running Corryvreckan beam check + noise masking (5000 events)..."
log_info  "Geometry in  : ${GEOM_INIT}"
log_info  "Geometry out : ${GEOM_OUT}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_INIT}|g" \
    -e "s|@GEOM_OUT@|${GEOM_OUT}|g" \
    template_beamcheck.conf > "${TMP_BEAMCHECK}"

${CORRY_EXEC} -c "${TMP_BEAMCHECK}"
rm -f "${TMP_BEAMCHECK}"

log_done "Corryvreckan done."
log_info  "ROOT file    : ${BEAMCHECK_ROOT}"
log_info  "Masked geom  : ${GEOM_OUT}"
echo ""

# ------------------------------------------------------------------------------
# Step 2: ROOT analysis + PNG
# ------------------------------------------------------------------------------
log_stage "Running beam center analysis (noise-filtered centroid)..."

MACRO="check_beam_center.C"
if [ -f "${MACRO}" ]; then
    MACRO_PATH="${MACRO}"
elif [ -f "../../DAQ/${MACRO}" ]; then
    MACRO_PATH="../../DAQ/${MACRO}"
else
    echo -e "\e[1;31m[ERROR]\e[0m ${MACRO} not found."
    exit 1
fi

set +e
root -l -b -q "${MACRO_PATH}(\"${BEAMCHECK_ROOT}\",\"${OUTPUT_PNG}\")"
BEAM_EXIT=$?
set -e

echo ""
echo -e "${CLR_DONE}=================================================${CLR_RESET}"
if [ "${BEAM_EXIT}" -ne 0 ]; then
    echo -e "${CLR_WARN}  RESULT : BEAM OFFSET > 3 mm on at least one sensor${CLR_RESET}"
else
    log_done "RESULT : Beam well-centered"
fi
log_done "PNG    : ${OUTPUT_PNG}"
log_done "Masks  : ${GEOM_OUT}"
echo -e "${CLR_DONE}=================================================${CLR_RESET}"
