#!/bin/bash

# ==============================================================================
#  MALTA2 Ithr Scan — Efficiency vs Ithr Plot
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
#
#  Usage:
#    ./run_ithr_scan.sh [ithr_scan.json]
#
#  JSON format (ithr_scan.json):
#    {
#      "runs": [
#        {"run": "002", "ithr": 10},
#        {"run": "008", "ithr": 30}
#      ]
#    }
#
#  If no JSON file is specified, ithr_scan.json in the current directory is used.
#  If the Analysis ROOT file for a run does not yet exist, Corryvreckan
#  analysis is run automatically before making the plot.
# ==============================================================================

set -e

# ---- Paths -------------------------------------------------------------------
CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"
DATA_DIR="/home/hinata/MALTA2/Ready_June/data"
OUTPUT_DIR="output"
MACRO_FILE="../../DAQ/plot_efficiency_vs_ithr.C"
DISCORD_WEBHOOK_URL="https://discord.com/api/webhooks/1435563629243269140/G1ATUg9qCZJcHr7A6WSFx4FEKRnHqFP1Xr1PCjcaB4Poos8nmprq3ocTi_iaHLjfSYDr"

# ---- Colors ------------------------------------------------------------------
CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_WARN="\e[1;33m"
CLR_ERR="\e[1;31m"
CLR_RESET="\e[0m"

log_stage() { echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"; }
log_done()  { echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"; }
log_info()  { echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"; }
log_warn()  { echo -e "${CLR_WARN}[$(date +'%T')][WARN]${CLR_RESET} $1"; }
log_err()   { echo -e "${CLR_ERR}[$(date +'%T')][ERROR]${CLR_RESET} $1"; }

# ---- JSON file ---------------------------------------------------------------
JSON_FILE="${1:-ithr_scan.json}"

if [ ! -f "${JSON_FILE}" ]; then
    log_err "JSON file not found: ${JSON_FILE}"
    echo -e "  Create it with format:"
    echo -e '  { "runs": [ {"run": "002", "ithr": 10}, {"run": "008", "ithr": 30} ] }'
    exit 1
fi

# ---- Parse JSON with Python3 → "RUN ITHR" lines -----------------------------
PARSED=$(python3 - "${JSON_FILE}" <<'PYEOF'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
for entry in data["runs"]:
    print(str(entry["run"]) + " " + str(entry["ithr"]))
PYEOF
)

if [ -z "${PARSED}" ]; then
    log_err "No entries found in ${JSON_FILE}."
    exit 1
fi

clear
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"
echo -e "  MALTA2 Efficiency vs Ithr Scan"
echo -e "  Config: ${JSON_FILE}"
echo -e "${CLR_INFO}=====================================================================${CLR_RESET}"
echo ""

mkdir -p "${OUTPUT_DIR}"

# Temporary pairs file passed to the ROOT macro
PAIRS_FILE=$(mktemp /tmp/ithr_pairs_XXXXXX.txt)
trap 'rm -f "${PAIRS_FILE}"' EXIT

# ---- Process each run --------------------------------------------------------
while IFS=" " read -r RUN_NUMBER ITHR; do
    [ -z "${RUN_NUMBER}" ] && continue

    ANALYSIS_ROOT="${OUTPUT_DIR}/Analysis_run${RUN_NUMBER}.root"

    if [ ! -f "${ANALYSIS_ROOT}" ]; then
        log_stage "No analysis file for run ${RUN_NUMBER}. Running Corryvreckan..."

        TMP_CONF=$(mktemp /tmp/tmp_analysis_XXXXXX.conf)
        sed -e "s|@RUN@|${RUN_NUMBER}|g" \
            -e "s|@REAL_DATA@|true|g" \
            -e "s|@BASE_PATH@|${DATA_DIR}|g" \
            template_analysis.conf > "${TMP_CONF}"

        ${CORRY_EXEC} -c "${TMP_CONF}"
        rm -f "${TMP_CONF}"

        if [ ! -f "${ANALYSIS_ROOT}" ]; then
            log_warn "Corryvreckan did not produce ${ANALYSIS_ROOT}. Skipping run ${RUN_NUMBER}."
            continue
        fi
        log_done "Corryvreckan done for run ${RUN_NUMBER}."
    else
        log_info "Using existing analysis: ${ANALYSIS_ROOT}"
    fi

    ABS_ROOT="$(realpath "${ANALYSIS_ROOT}")"
    echo "${ITHR} ${ABS_ROOT}" >> "${PAIRS_FILE}"
    log_info "Registered: Ithr=${ITHR}  run=${RUN_NUMBER}"
done <<< "${PARSED}"

# ---- Check we have at least one valid point ----------------------------------
if [ ! -s "${PAIRS_FILE}" ]; then
    log_err "No valid (Ithr, ROOT file) pairs collected. Aborting."
    exit 1
fi

echo ""
log_stage "Collected pairs:"
cat "${PAIRS_FILE}"
echo ""

# ---- Output plot path --------------------------------------------------------
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPNG="${OUTPUT_DIR}/efficiency_vs_ithr_${TIMESTAMP}.png"
ABS_OUTPNG="$(realpath "$(dirname "${OUTPNG}")")/$(basename "${OUTPNG}")"

# ---- Run ROOT macro ----------------------------------------------------------
log_stage "Generating efficiency vs Ithr plot..."

if [ ! -f "${MACRO_FILE}" ]; then
    log_err "ROOT macro not found: ${MACRO_FILE}"
    exit 1
fi

root -l -b -q "${MACRO_FILE}(\"${PAIRS_FILE}\",\"${ABS_OUTPNG}\",\"MALTA_1\")"

log_done "Plot saved: ${OUTPNG}"
echo ""

# ---- Discord notification ----------------------------------------------------
log_stage "Sending result to Discord..."

curl -s \
    -F "payload_json={\"content\": \"**[MALTA2 Ithr Scan]** Efficiency vs Ithr plot ready :chart_with_upwards_trend: (${TIMESTAMP})\"}" \
    -F "files[0]=@${ABS_OUTPNG}" \
    "${DISCORD_WEBHOOK_URL}" > /dev/null

log_done "Discord notification sent."
echo ""

echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
log_done "Ithr scan complete."
log_done "Plot: ${OUTPNG}"
echo -e "${CLR_DONE}=====================================================================${CLR_RESET}"
