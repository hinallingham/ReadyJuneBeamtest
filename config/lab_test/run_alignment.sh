#!/bin/bash

# ==============================================================================
#  MALTA2 Telescope 3-Plane Alignment & Analysis Pipeline
#  System Architecture & Automation Suite
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================

set -e

# Core Parameters
RUN_NUMBER=${1:-"002"}
PED_RUN=${2:-"001"}

# Executable Path Bind
CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"

# Discord Webhook Grid Bindings
DISCORD_WEBHOOK_URL="https://discord.com/api/webhooks/1435563629243269140/G1ATUg9qCZJcHr7A6WSFx4FEKRnHqFP1Xr1PCjcaB4Poos8nmprq3ocTi_iaHLjfSYDr"
SCRIPT_NAME=$(basename "$0")
USERNAME="allpix-runner@$(hostname)"

# Filesystem Topology
GEOM_DIR="../../geometry/3malta_ref_30tilt"
OUTPUT_DIR="../../output"
mkdir -p "${OUTPUT_DIR}"

# Geometry Pipeline Tracking
GEOM_INIT="${GEOM_DIR}/3malta_init.conf"
GEOM_BEAMCHECK="${GEOM_DIR}/3malta_beamcheck_masked.conf"
GEOM_PED_MASKED="${GEOM_DIR}/3malta_ped_masked.conf"
GEOM_PREALIGNED="${GEOM_DIR}/3malta_prealigned.conf"
GEOM_ALIGNED="${GEOM_DIR}/3malta_aligned.conf"

# Buffer Streams
TMP_PED="tmp_pedestal.conf"
TMP_PREALIGN="tmp_prealign.conf"
TMP_ALIGN="tmp_align.conf"
TMP_ALIGNCHECK="tmp_allaligncheck.conf"
TMP_ANALYSI="tmp_analysis.conf"

# Terminal Color Codes
CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_RESET="\e[0m"

log_stage() {
    echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"
}

log_done() {
    echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"
}

log_info() {
    echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"
}

clear
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"
echo -e "  CORRYVRECKAN AUTOMATED PIPELINE: MULTI-STAGE TRACKING ALIGNMENT    "
echo -e "  Target Core Dataset : Run ${RUN_NUMBER} (Beam Interaction Mode)     "
echo -e "  Baseline Pedestal   : Run ${PED_RUN} (Static Noise Map)           "
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"

# ------------------------------------------------------------------------------
# PHASE 1: Static Noise Extraction
# ------------------------------------------------------------------------------
log_stage "Executing Phase 1: Frequency-based Pedestal Masking..."

if [ -f "${GEOM_BEAMCHECK}" ]; then
    GEOM_PHASE1_IN="${GEOM_BEAMCHECK}"
    log_info "Source Map: ${GEOM_PHASE1_IN}  [beamcheck masks propagated]"
else
    GEOM_PHASE1_IN="${GEOM_INIT}"
    log_info "Source Map: ${GEOM_PHASE1_IN}"
fi

sed -e "s|@RUN@|${PED_RUN}|g" \
    -e "s|@GEOM_IN@|${GEOM_PHASE1_IN}|g" \
    -e "s|@GEOM_OUT@|${GEOM_PED_MASKED}|g" \
    template_mask.conf > "${TMP_PED}"

${CORRY_EXEC} -c "${TMP_PED}"
rm -f "${TMP_PED}"

log_done "Phase 1 complete. Static matrix exported to: ${GEOM_PED_MASKED}"
echo ""

# ------------------------------------------------------------------------------
# PHASE 2: Macro-scale Structural Correction
# ------------------------------------------------------------------------------
log_stage "Executing Phase 2: Spatial Coarse-graining Prealignment..."
log_info "Source Map: ${GEOM_PED_MASKED}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_PED_MASKED}|g" \
    -e "s|@GEOM_OUT@|${GEOM_PREALIGNED}|g" \
    template_prealign.conf > "${TMP_PREALIGN}"

${CORRY_EXEC} -c "${TMP_PREALIGN}"
rm -f "${TMP_PREALIGN}"

log_done "Phase 2 complete. Initial displacement localized: ${GEOM_PREALIGNED}"
echo ""

# ------------------------------------------------------------------------------
# PHASE 3: Micro-scale Matrix Minimization
# ------------------------------------------------------------------------------
log_stage "Executing Phase 3: High-Precision Millepede Global Alignment..."
log_info "Source Map: ${GEOM_PREALIGNED}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_PREALIGNED}|g" \
    -e "s|@GEOM_OUT@|${GEOM_ALIGNED}|g" \
    template_align.conf > "${TMP_ALIGN}"

${CORRY_EXEC} -c "${TMP_ALIGN}"
rm -f "${TMP_ALIGN}"

log_done "Phase 3 complete. Master Aligned Topology State locked in: ${GEOM_ALIGNED}"
echo ""

# ------------------------------------------------------------------------------
# PHASE 3.5: Post-Alignment QC Check (Correlations 2D + Residuals)
# ------------------------------------------------------------------------------
log_stage "Executing Phase 3.5: Post-Alignment Correlation & Residual Check..."
log_info "Source Map: ${GEOM_ALIGNED}"

ALIGNCHECK_DIR="output/allaligncheck"
mkdir -p "${ALIGNCHECK_DIR}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_ALIGNED}|g" \
    template_allaligncheck.conf > "${TMP_ALIGNCHECK}"

${CORRY_EXEC} -c "${TMP_ALIGNCHECK}"
rm -f "${TMP_ALIGNCHECK}"

ALIGNCHECK_ROOT="output/allaligncheck_run${RUN_NUMBER}.root"
CORR2D_PNG="${ALIGNCHECK_DIR}/correlation2D_run${RUN_NUMBER}.png"
RESIDUALS_PNG="${ALIGNCHECK_DIR}/residuals_run${RUN_NUMBER}.png"

for MACRO_FILE in "check_correlation2D.C" "check_residuals.C"; do
    if [ -f "${MACRO_FILE}" ]; then
        MACRO_FOUND="${MACRO_FILE}"
    elif [ -f "../../DAQ/${MACRO_FILE}" ]; then
        MACRO_FOUND="../../DAQ/${MACRO_FILE}"
    else
        echo -e "\e[1;31m[ERROR]\e[0m ${MACRO_FILE} not found."
        exit 1
    fi
    if [ "${MACRO_FILE}" = "check_correlation2D.C" ]; then
        root -l -b -q "${MACRO_FOUND}(\"${ALIGNCHECK_ROOT}\",\"${CORR2D_PNG}\")"
    else
        root -l -b -q "${MACRO_FOUND}(\"${ALIGNCHECK_ROOT}\",\"${RESIDUALS_PNG}\")"
    fi
done

log_done "Phase 3.5 complete. QC plots: ${CORR2D_PNG}  ${RESIDUALS_PNG}"
echo ""

# ------------------------------------------------------------------------------
# SYSTEM TERMINATION SIGNALS
# ------------------------------------------------------------------------------
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
log_done "All pipelines complete. Masking -> Prealignment -> Alignment -> QC done."
log_done "Correlation2D : ${CORR2D_PNG}"
log_done "Residuals     : ${RESIDUALS_PNG}"
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"