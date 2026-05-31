#!/bin/bash

# ==============================================================================
#  MALTA2 Online Monitor
#  Inherits the best available geometry: aligned > ped_masked > beamcheck > init
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================

RUN_NUMBER=${1:-"001"}

CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"
GEOM_DIR="../../geometry/3malta_ref"
GEOM_INIT="${GEOM_DIR}/3malta_init.conf"
GEOM_BEAMCHECK="${GEOM_DIR}/3malta_beamcheck_masked.conf"
GEOM_PED_MASKED="${GEOM_DIR}/3malta_ped_masked.conf"
GEOM_ALIGNED="${GEOM_DIR}/3malta_aligned.conf"

TMP_MONITOR="tmp_onlinemonitor.conf"

CLR_INFO="\e[1;34m"
CLR_DONE="\e[1;32m"
CLR_WARN="\e[1;33m"
CLR_RESET="\e[0m"

log_info() { echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"; }
log_done() { echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"; }
log_warn() { echo -e "${CLR_WARN}[$(date +'%T')][WARN]${CLR_RESET} $1"; }

clear
echo -e "${CLR_INFO}=================================================${CLR_RESET}"
echo -e "  MALTA2 ONLINE MONITOR  --  Run ${RUN_NUMBER}            "
echo -e "${CLR_INFO}=================================================${CLR_RESET}"

# Select best available geometry (aligned > ped_masked > beamcheck > init)
if [ -f "${GEOM_ALIGNED}" ]; then
    GEOM_MONITOR="${GEOM_ALIGNED}"
    GEOM_LABEL="aligned (full alignment applied)"
elif [ -f "${GEOM_PED_MASKED}" ]; then
    GEOM_MONITOR="${GEOM_PED_MASKED}"
    GEOM_LABEL="pedestal masked (not yet aligned)"
    log_warn "Aligned geometry not found. Using pedestal masked geometry."
elif [ -f "${GEOM_BEAMCHECK}" ]; then
    GEOM_MONITOR="${GEOM_BEAMCHECK}"
    GEOM_LABEL="beamcheck masked (no alignment)"
    log_warn "Using beamcheck geometry only. Run alignment for better results."
else
    GEOM_MONITOR="${GEOM_INIT}"
    GEOM_LABEL="initial geometry (no masks, no alignment)"
    log_warn "Using initial geometry. Run beamcheck and alignment first."
fi

log_info "Geometry : ${GEOM_MONITOR}"
log_info "Status   : ${GEOM_LABEL}"
log_info "Run      : ${RUN_NUMBER}"
echo ""

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_MONITOR}|g" \
    template_onlinemonitor.conf > "${TMP_MONITOR}"

trap "rm -f ${TMP_MONITOR}; echo ''; log_done 'Online monitor stopped.'" EXIT

${CORRY_EXEC} -c "${TMP_MONITOR}"
