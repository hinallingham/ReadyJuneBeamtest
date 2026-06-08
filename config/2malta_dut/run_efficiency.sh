#!/bin/bash

# ==============================================================================
#  MALTA2 Telescope Efficiency Analysis Pipeline
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================

set -e

# Core Parameters
RUN_NUMBER=${1:-"002"}

# Executable Path
CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"

# Discord Webhook
DISCORD_WEBHOOK_URL="https://discord.com/api/webhooks/1435563629243269140/G1ATUg9qCZJcHr7A6WSFx4FEKRnHqFP1Xr1PCjcaB4Poos8nmprq3ocTi_iaHLjfSYDr"

# Data Path
DATA_DIR="/home/MaltaSW/MaltaDAQ/TelescopeData"

# Filesystem Topology
OUTPUT_DIR="output"
EFFICIENCY_DIR="${OUTPUT_DIR}/efficiency/efficiency_run${RUN_NUMBER}"
mkdir -p "${EFFICIENCY_DIR}"

# Temporary config
TMP_ANALYSIS="tmp_analysis.conf"

# Analysis output ROOT file (produced in working directory by corry)
ANALYSIS_ROOT="${OUTPUT_DIR}/Analysis_run${RUN_NUMBER}.root"

# Terminal Color Codes
CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_RESET="\e[0m"

log_stage() { echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"; }
box_done()  { echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"; }
log_info()  { echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"; }

clear
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"
echo -e "  CORRYVRECKAN EFFICIENCY ANALYSIS PIPELINE                           "
echo -e "  Target Run: ${RUN_NUMBER}                                           "
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"

# ------------------------------------------------------------------------------
# PHASE 1: Run Corryvreckan Analysis
# ------------------------------------------------------------------------------
log_stage "Executing Corryvreckan efficiency analysis for run ${RUN_NUMBER}..."
log_info  "Geometry: geometry/2malta_dut/2maltaDUT_aligned.conf  [post-alignment]"
log_info  "Data:     ${DATA_DIR}"

sed -e "s|@RUN@|${RUN_NUMBER}|g" \
    -e "s|@REAL_DATA@|true|g" \
    -e "s|@BASE_PATH@|${DATA_DIR}|g" \
    template_analysis.conf > "${TMP_ANALYSIS}"

${CORRY_EXEC} -c "${TMP_ANALYSIS}"
rm -f "${TMP_ANALYSIS}"

box_done "Corryvreckan done. Output: ${ANALYSIS_ROOT}"
echo ""

# ------------------------------------------------------------------------------
# PHASE 2: Generate Efficiency PNG Plots
# ------------------------------------------------------------------------------
log_stage "Generating efficiency plots from ${ANALYSIS_ROOT}..."

MACRO_FILE="make_efficiency_plots.C"
if [ ! -f "${MACRO_FILE}" ]; then
    echo -e "\e[1;31m[ERROR]\e[0m ${MACRO_FILE} not found."
    exit 1
fi

root -l -b -q "${MACRO_FILE}(\"${ANALYSIS_ROOT}\",\"${EFFICIENCY_DIR}/\")"

box_done "All efficiency plots saved to ${EFFICIENCY_DIR}/"
echo ""

# ------------------------------------------------------------------------------
# PHASE 3: Discord Notification
# ------------------------------------------------------------------------------
log_stage "Sending results to Discord..."

# Collect key plots to send (chipEfficiencyMap + totalEfficiency per DUT)
DISCORD_FILES=()
for f in "${EFFICIENCY_DIR}"/*chipEfficiencyMap_trackPos_TProfile.png \
          "${EFFICIENCY_DIR}"/*eTotalEfficiency.png \
          "${EFFICIENCY_DIR}"/*pixelEfficiencyMap_trackPos_TProfile.png \
          "${EFFICIENCY_DIR}"/*efficiencyColumns.png \
          "${EFFICIENCY_DIR}"/*efficiencyRows.png; do
    [ -f "$f" ] && DISCORD_FILES+=("$f")
done

# Build curl file arguments (Discord: max 10 files)
CURL_FILES=()
count=0
for f in "${DISCORD_FILES[@]}"; do
    [ $count -ge 10 ] && break
    CURL_FILES+=(-F "files[${count}]=@${f}")
    count=$((count + 1))
done

curl -s \
    -F "payload_json={\"content\": \"**[MALTA2 Efficiency]** Run \`${RUN_NUMBER}\` :bar_chart: **Complete** — ${count} key plots attached\"}" \
    "${CURL_FILES[@]}" \
    "${DISCORD_WEBHOOK_URL}" > /dev/null

box_done "Discord notification sent (${count} plots)."
echo ""

# ------------------------------------------------------------------------------
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
box_done "Efficiency analysis complete."
box_done "ROOT file : ${ANALYSIS_ROOT}"
box_done "PNG plots : ${EFFICIENCY_DIR}/"
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
