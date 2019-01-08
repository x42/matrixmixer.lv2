#!/bin/bash
N_INPUTS=$1
N_OUTPUTS=$2

NCTRL=$(( $N_INPUTS * $N_OUTPUTS ))
NPORTS=$(( $N_INPUTS + $N_OUTPUTS + $NCTRL ))

cat << EOF
extern const LV2_Descriptor* lv2_descriptor(uint32_t index);
extern const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index);

static const RtkLv2Description _plugin = {
	&lv2_descriptor, &lv2ui_descriptor
	, 0 // uint32_t dsp_descriptor_id
	, 0 // uint32_t gui_descriptor_id
	, "Matrix Mixer ${N_INPUTS}x${N_OUTPUTS}"
	, (const struct LV2Port[${NPORTS}])
	{
EOF

i=1; while test $i -le $N_INPUTS; do
	echo "		{ \"in$i\", AUDIO_IN, nan, nan, nan, \"Audio Input $i\"},"
	i=$[$i + 1]
done
i=1; while test $i -le $N_OUTPUTS; do
	echo "		{ \"out$i\", AUDIO_OUT, nan, nan, nan, \"Audio Output $i\"},"
	i=$[$i + 1]
done

i=1; while test $i -le $N_INPUTS; do
	j=1; while test $j -le $N_OUTPUTS; do
		AMP="0.f"
		if test $j -eq $i; then AMP="1.f"; fi
		echo "		{ \"mix_${i}_${j}\", CONTROL_IN, ${AMP}, -6.f, 6.f,\"Mix ${i} ${j}\"},"
		j=$[$j + 1]
	done
	i=$[$i + 1]
done

cat << EOF
	}
	, ${NPORTS} // uint32_t nports_total
	, ${N_INPUTS} // uint32_t nports_audio_in
	, ${N_OUTPUTS} // uint32_t nports_audio_out
	, 0 // uint32_t nports_midi_in
	, 0 // uint32_t nports_midi_out
	, 0 // uint32_t nports_atom_in
	, 0 // uint32_t nports_atom_out
	, ${NCTRL} // uint32_t nports_ctrl
	, ${NCTRL} // uint32_t nports_ctrl_in
	, 0 // uint32_t nports_ctrl_out
	, 8192 // uint32_t min_atom_bufsiz
	, false // bool send_time_info
	, UINT32_MAX // uint32_t latency_ctrl_port
};

EOF
