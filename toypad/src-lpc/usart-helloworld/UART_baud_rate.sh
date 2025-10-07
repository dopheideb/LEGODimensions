#!/bin/bash

PCLK=$[ 12 * 1000 * 1000 ]
UART_REQUESTED_BAUDRATE=115200

uart_baud_rate()
{
	PCLK="$1"
	U0DLM="$2"
	U0DLL="$3"
	DIVADDVAL="$4"
	MULVAL="$5"
	#echo "[DEBUG] $PCLK $U0DLM $U0DLL $DIVADDVAL $MULVAL"
	cat <<-__EOT__ | bc
		scale=5
		${PCLK} / (16 * (256 * ${U0DLM} + ${U0DLL}) * (1 + ${DIVADDVAL}/${MULVAL}))
	__EOT__
}

U0DLM=$[ ${PCLK} / (16 * 256 * ${UART_REQUESTED_BAUDRATE}) ]
if [ ${U0DLM} -ne 0 ]
then
	let --U0DLM
fi
echo "U0DLM=${U0DLM}"

U0DLL=$[ ${PCLK} / (${UART_REQUESTED_BAUDRATE} * (16 * (256 * ${U0DLM} + 1))) ]
let --U0DLL
echo "U0DLL=${U0DLL}"

for MULVAL in {1..15}
do
	for DIVADDVAL in {0..14}
	do
		BAUD_RATE="$(uart_baud_rate "${PCLK}" "${U0DLM}" "${U0DLL}" "${DIVADDVAL}" "${MULVAL}")"
		echo "${BAUD_RATE} U0DLM=${U0DLM} U0DLL=${U0DLL} DIVADDVAL=${DIVADDVAL} MULVAL=${MULVAL}"
	done
done | sort -n
