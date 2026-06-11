#!/bin/bash
# ==============================================================================
#  MALTA2 Beam Check — Batch All Runs
#  Author: Hinata Nakamura (Quark Physics Laboratory, Hiroshima University)
# ==============================================================================
#
#  Usage:
#    cd /home/hinata/MALTA2/Ready_June/config/2malta_dut
#    ./run_beamcheck_all.sh
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# Paths
# ------------------------------------------------------------------------------
DATA_DIR="/home/hinata/MALTA2/Ready_June/data"
CORRY_EXEC="/home/hinata/package/corryvreckan/bin/corry"
GEOM_DIR="../../geometry/2malta_dut"
GEOM_INIT="${GEOM_DIR}/2maltaDUT_init.conf"
GEOM_OUT="${GEOM_DIR}/2maltaDUT_beamcheck_masked.conf"
OUTPUT_DIR="output/beamcheck"

DISCORD_WEBHOOK_URL="https://discord.com/api/webhooks/1435563629243269140/G1ATUg9qCZJcHr7A6WSFx4FEKRnHqFP1Xr1PCjcaB4Poos8nmprq3ocTi_iaHLjfSYDr"

# ------------------------------------------------------------------------------
# Logging
# ------------------------------------------------------------------------------
CLR_STAGE="\e[1;36m"
CLR_DONE="\e[1;32m"
CLR_INFO="\e[1;34m"
CLR_WARN="\e[1;33m"
CLR_ERROR="\e[1;31m"
CLR_RESET="\e[0m"

log_stage() { echo -e "${CLR_STAGE}[$(date +'%T')][STAGE]${CLR_RESET} $1"; }
log_done()  { echo -e "${CLR_DONE}[$(date +'%T')][DONE]${CLR_RESET} $1"; }
log_info()  { echo -e "${CLR_INFO}[$(date +'%T')][INFO]${CLR_RESET} $1"; }
log_warn()  { echo -e "${CLR_WARN}[$(date +'%T')][WARN]${CLR_RESET} $1"; }
log_error() { echo -e "${CLR_ERROR}[$(date +'%T')][ERROR]${CLR_RESET} $1"; }

# ------------------------------------------------------------------------------
# Collect unique run numbers from data directory
# ------------------------------------------------------------------------------
mapfile -t RUN_NUMBERS < <(
    ls "${DATA_DIR}"/run_*.root 2>/dev/null \
    | sed -E 's|.*/run_([0-9]+)_[0-9]+\.root|\1|' \
    | awk '{printf "%d\n", $0+0}' \
    | sort -un
)

if [ ${#RUN_NUMBERS[@]} -eq 0 ]; then
    log_error "No run_*.root files found in ${DATA_DIR}"
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

TOTAL=${#RUN_NUMBERS[@]}
N_PASS=0
N_FAIL=0
RESULTS=()

clear
echo -e "${CLR_INFO}=================================================${CLR_RESET}"
echo -e "  MALTA2 BATCH BEAM CHECK  --  ${TOTAL} runs found"
echo -e "${CLR_INFO}=================================================${CLR_RESET}"
echo ""

# ------------------------------------------------------------------------------
# Main loop
# ------------------------------------------------------------------------------
for i in "${!RUN_NUMBERS[@]}"; do
    RUN_NUMBER="${RUN_NUMBERS[$i]}"
    IDX=$((i + 1))

    BEAMCHECK_ROOT="output/beamcheck_run${RUN_NUMBER}.root"
    OUTPUT_PNG="${OUTPUT_DIR}/beamcheck_run${RUN_NUMBER}.png"
    TMP_CONF="tmp_beamcheck_${RUN_NUMBER}.conf"

    echo ""
    echo -e "${CLR_INFO}┌─ Run ${RUN_NUMBER}  (${IDX}/${TOTAL}) ──────────────────────────${CLR_RESET}"

    # Step 1: Generate config and run Corryvreckan
    log_stage "Running Corryvreckan..."
    sed -e "s|@RUN@|${RUN_NUMBER}|g" \
        -e "s|@GEOM_IN@|${GEOM_INIT}|g" \
        -e "s|@GEOM_OUT@|${GEOM_OUT}|g" \
        template_beamcheck.conf > "${TMP_CONF}"

    set +e
    ${CORRY_EXEC} -c "${TMP_CONF}" 2>&1
    CORRY_EXIT=$?
    set -e
    rm -f "${TMP_CONF}"

    if [ "${CORRY_EXIT}" -ne 0 ]; then
        log_error "Corryvreckan failed for run ${RUN_NUMBER} (exit ${CORRY_EXIT})"
        N_FAIL=$((N_FAIL + 1))
        RESULTS+=("${RUN_NUMBER} CORRY_FAIL")
        echo -e "${CLR_INFO}└────────────────────────────────────────────────${CLR_RESET}"
        continue
    fi
    log_done "Corryvreckan done → ${BEAMCHECK_ROOT}"

    # Step 2: ROOT beam center analysis
    log_stage "Running beam center analysis..."
    MACRO="check_beam_center.C"
    if [ -f "${MACRO}" ]; then
        MACRO_PATH="${MACRO}"
    elif [ -f "../../DAQ/${MACRO}" ]; then
        MACRO_PATH="../../DAQ/${MACRO}"
    else
        log_error "${MACRO} not found"
        N_FAIL=$((N_FAIL + 1))
        RESULTS+=("${RUN_NUMBER} MACRO_MISSING")
        echo -e "${CLR_INFO}└────────────────────────────────────────────────${CLR_RESET}"
        continue
    fi

    set +e
    root -l -b -q "${MACRO_PATH}(\"${BEAMCHECK_ROOT}\",\"${OUTPUT_PNG}\")"
    BEAM_EXIT=$?
    set -e

    if [ "${BEAM_EXIT}" -ne 0 ]; then
        log_warn "Beam offset > 3 mm on at least one sensor"
        N_FAIL=$((N_FAIL + 1))
        RESULTS+=("${RUN_NUMBER} BEAM_OFFSET")
    else
        log_done "Beam well-centered"
        N_PASS=$((N_PASS + 1))
        RESULTS+=("${RUN_NUMBER} PASS")
    fi

    echo -e "${CLR_INFO}└────────────────────────────────────────────────${CLR_RESET}"
done

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
echo ""
echo -e "${CLR_DONE}=================================================${CLR_RESET}"
echo -e "  BATCH COMPLETE"
echo -e "  Total : ${TOTAL}   Pass : ${N_PASS}   Fail : ${N_FAIL}"
echo -e "${CLR_DONE}=================================================${CLR_RESET}"
echo ""
printf "  %-8s %s\n" "RUN" "STATUS"
printf "  %-8s %s\n" "---" "------"
for entry in "${RESULTS[@]}"; do
    run=$(echo "${entry}"    | awk '{print $1}')
    status=$(echo "${entry}" | awk '{print $2}')
    case "${status}" in
        PASS)  clr="${CLR_DONE}" ;;
        *)     clr="${CLR_ERROR}" ;;
    esac
    printf "  %-8s " "${run}"
    echo -e "${clr}${status}${CLR_RESET}"
done
echo ""

# ------------------------------------------------------------------------------
# Coincidence rate plot
# ------------------------------------------------------------------------------
log_stage "Generating coincidence rate vs run plot..."
COINC_MACRO="../../DAQ/plot_coincidence_rate_vs_run.C"
COINC_PNG="../../DAQ/coincidence_rate_vs_run.png"
if [ -f "${COINC_MACRO}" ]; then
    root -l -b -q "${COINC_MACRO}(\"$(pwd)/output\",\"$(realpath "$(pwd)/../../DAQ")/coincidence_rate_vs_run.png\")"
    log_done "Coincidence rate plot saved to ${COINC_PNG}"
else
    log_warn "plot_coincidence_rate_vs_run.C not found — skipping"
fi
echo ""

# ------------------------------------------------------------------------------
# Discord summary
# ------------------------------------------------------------------------------
log_stage "Sending batch summary to Discord..."

DISCORD_MSG="**[MALTA2 Batch Beam Check]** ${TOTAL} runs — ✅ Pass: ${N_PASS}  ❌ Fail: ${N_FAIL}\n"
for entry in "${RESULTS[@]}"; do
    run=$(echo "${entry}"    | awk '{print $1}')
    status=$(echo "${entry}" | awk '{print $2}')
    case "${status}" in
        PASS)         icon=":white_check_mark:" ;;
        BEAM_OFFSET)  icon=":warning:" ;;
        *)            icon=":x:" ;;
    esac
    DISCORD_MSG+="  ${icon} Run \`${run}\`: ${status}\n"
done

curl -s \
    -H "Content-Type: application/json" \
    -d "{\"content\": \"$(echo -e "${DISCORD_MSG}" | sed 's/"/\\"/g')\"}" \
    "${DISCORD_WEBHOOK_URL}" > /dev/null

log_done "Discord notification sent."
