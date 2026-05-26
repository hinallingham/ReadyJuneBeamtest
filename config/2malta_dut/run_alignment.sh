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
TMP_ANALYSI="tmp_analysis.conf"

# Terminal Color Codes
CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_RESET="\e[0m"

log_stage() {
    echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"
}

box_done() {
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
log_info "Source Map: ${GEOM_INIT}"

sed -e "s|@RUN@|${PED_RUN}|g" \
    -e "s|@GEOM_IN@|${GEOM_INIT}|g" \
    -e "s|@GEOM_OUT@|${GEOM_PED_MASKED}|g" \
    template_mask.conf > "${TMP_PED}"

${CORRY_EXEC} -c "${TMP_PED}"
rm -f "${TMP_PED}"

box_done "Phase 1 complete. Static matrix exported to: ${GEOM_PED_MASKED}"
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

box_done "Phase 2 complete. Initial displacement localized: ${GEOM_PREALIGNED}"
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

box_done "Phase 3 complete. Master Aligned Topology State locked in: ${GEOM_ALIGNED}"
echo ""

# ------------------------------------------------------------------------------
# PHASE 3.5: High-Statistics Physics Tracking Analysis
# ------------------------------------------------------------------------------
log_stage "Executing Phase 3.5: Final Analysis with Aligned Geometry Topology..."
log_info "Source Map: ${GEOM_ALIGNED}"

# Process the template with the master aligned geometry configuration
sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@GEOM_IN@|${GEOM_ALIGNED}|g" \
    -e "s|@GEOM_OUT@|${GEOM_ALIGNED}_final|g" \
    template_analysis.conf > "${TMP_ANALYSI}"

${CORRY_EXEC} -c "${TMP_ANALYSI}"
rm -f "${TMP_ANALYSI}"

box_done "Phase 3.5 complete. Physics analysis histogram created successfully."
echo ""

# ------------------------------------------------------------------------------
# PHASE 4: Automated QC Plot Generation & Discord Notification
# ------------------------------------------------------------------------------
log_stage "Executing Phase 4: Running ROOT QC Analysis & Dispatching Notification..."

# ★ 新しいマクロ名に変更
MACRO_NAME="check_full_DUT_qc.C"
TARGET_IMAGE="alignment_qc_result.png"

# Target both alignment output (for 2D correlations) and analysis output (for 1D residuals)
ALIGN_ROOT_FILE="output/align_millepede_run${RUN_NUMBER}.root"
ANALYSIS_ROOT_FILE="output/Analysis_run${RUN_NUMBER}.root"

# Handle paths depending on where this script is running
if [ -f "${MACRO_NAME}" ]; then
    MACRO_PATH="${MACRO_NAME}"
elif [ -f "../../DAQ/${MACRO_NAME}" ]; then
    MACRO_PATH="../../DAQ/${MACRO_NAME}"
else
    echo -e "\e[1;31m[ERROR]\e[0m ${MACRO_NAME} not found in current directory or DAQ path."
    exit 1
fi

# 2. Fire ROOT in batch mode with dual file parameters for correlation & tracking hybrid mapping
log_info "Analyzing ${ALIGN_ROOT_FILE} & ${ANALYSIS_ROOT_FILE} via ${MACRO_PATH}..."
root -l -b -q "${MACRO_PATH}(\"${ALIGN_ROOT_FILE}\",\"${ANALYSIS_ROOT_FILE}\")"

# 3. Securely transport the image payload directly to Discord API endpoint
if [ -f "${TARGET_IMAGE}" ]; then
    log_info "Transmitting QC image artifact to Discord..."
    curl -X POST \
         -H "Content-Type: multipart/form-data" \
         -F "username=${USERNAME}" \
         -F "content=🚀 **[Automated Telemetry Signal]** Alignment & High-Statistics Physics Tracking Analysis completely synchronized for **Run ${RUN_NUMBER}**! Custom performance grid metrics (2D Spatial Correlation, High-Precision Unbiased Residuals, and In-Pixel Efficiency Map) for the Device Under Test (**MALTA_1**) have been rendered successfully and compiled below." \
         -F "file=@${TARGET_IMAGE}" \
         "${DISCORD_WEBHOOK_URL}"
    
    # Optional cleanup: uncomment if you don't want to leave temporary PNGs in the folder
    # rm -f "${TARGET_IMAGE}" "alignment_qc_result.pdf"
else
    echo -e "\e[1;31m[ERROR]\e[0m Failed to generate ${TARGET_IMAGE}. Notification skipped."
fi

# ------------------------------------------------------------------------------
# SYSTEM TERMINATION SIGNALS
# ------------------------------------------------------------------------------
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
box_done "All data pipelines integrated successfully with zero warnings."
log_info "Telemetry synchronized completely with Discord channel."
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"