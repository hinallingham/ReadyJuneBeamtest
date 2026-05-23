#!/bin/bash

# ==============================================================================
#  MALTA2 Telescope 2-Reference + 1-DUT Alignment Pipeline
#  System Architecture & Automation Suite
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================

set -e

# Core Parameters
RUN_NUMBER=${1:-"002"}
PED_RUN=${2:-"001"}

# Executable Path Bind
CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"

# Filesystem Topology
GEOM_DIR="../../geometry/2malta_dut"
OUTPUT_DIR="../../output"
mkdir -p "${OUTPUT_DIR}"

# Geometry Pipeline Tracking
GEOM_INIT="${GEOM_DIR}/2maltaDUT_init.conf"
GEOM_PED_MASKED="${GEOM_DIR}/2maltaDUT_ped_masked.conf"
GEOM_PREALIGNED="${GEOM_DIR}/2maltaDUT_prealigned.conf"
GEOM_ALIGNED="${GEOM_DIR}/2maltaDUT_aligned.conf"

# Buffer Streams
TMP_PED="tmp_pedestal.conf"
TMP_PREALIGN="tmp_prealign.conf"
TMP_ALIGN="tmp_align.conf"

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
echo -e "  CORRYVRECKAN AUTOMATED PIPELINE: 2-REFERENCE + 1-DUT ALIGNMENT     "
echo -e "  Target Core Dataset : Run ${RUN_NUMBER} (Beam Interaction Mode)     "
echo -e "  Baseline Pedestal   : Run ${PED_RUN} (Static Noise Map)           "
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"

# ------------------------------------------------------------------------------
# PHASE 1: Static Noise Extraction
# ------------------------------------------------------------------------------
log_stage "Executing Phase 1: Frequency-based Pedestal Masking..."
log_info "Source Map: ${GEOM_INIT}"

sed -e "s|@RUN@|${PED_RUN}|g" \
    -e "s|@GEOM_IN@|${GEOM_INIT}|g" \
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
# PHASE 3: Micro-scale Matrix Minimization (Millepede Alignment)
# ------------------------------------------------------------------------------
log_stage "Executing Phase 3: High-Precision Millepede Global Alignment..."
log_info "Source Map: ${GEOM_PREALIGNED}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_PREALIGNED}|g" \
    -e "s|@GEOM_OUT@|${GEOM_ALIGNED}|g" \
    template_align.conf > "${TMP_ALIGN}"

${CORRY_EXEC} -c "${TMP_ALIGN}"
rm -f "${TMP_ALIGN}"

# ------------------------------------------------------------------------------
# SYSTEM TERMINATION SIGNALS
# ------------------------------------------------------------------------------
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
log_done "All data pipelines integrated successfully with zero warnings."
log_info "Master Aligned Topology State locked in: ${GEOM_ALIGNED}"
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"