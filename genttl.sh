#!/usr/bin/env bash
N_INPUTS=$1
N_OUTPUTS=$2

URISUFFIX=i${N_INPUTS}o${N_OUTPUTS}

IDX=0

echo
echo "lv2:port ["

i=1; while test $i -le $N_INPUTS; do
if test $IDX -gt 0; then
	echo "  ] , ["
fi
cat << EOF
    a lv2:AudioPort ,
      lv2:InputPort ;
    lv2:index ${IDX} ;
    lv2:symbol "in${i}" ;
    lv2:name "Audio Input ${i}" ;
EOF
	i=$[$i + 1]
	IDX=$[$IDX + 1]
done

i=1; while test $i -le $N_OUTPUTS; do
cat << EOF
  ] , [
    a lv2:AudioPort ,
      lv2:OutputPort ;
    lv2:index ${IDX} ;
    lv2:symbol "out${i}" ;
    lv2:name "Audio Output ${i}" ;
EOF
	i=$[$i + 1]
	IDX=$[$IDX + 1]
done

i=1; while test $i -le $N_INPUTS; do
	j=1; while test $j -le $N_OUTPUTS; do
	AMP=0.0
	if test $j -eq $i; then AMP=1.0; fi
cat << EOF
  ] , [
    a lv2:ControlPort ,
      lv2:InputPort ;
    lv2:index ${IDX} ;
    lv2:symbol "mix_${i}_${j}" ;
    lv2:name "Mix ${i} ${j}" ;
    pg:group matrixmixer:g$i;
    lv2:minimum -6.0;
    lv2:maximum  6.0;
    lv2:default  ${AMP};
    lv2:scalePoint [ rdfs:label "invert"; rdf:value -1.0 ; ] ;
    lv2:scalePoint [ rdfs:label "off"; rdf:value 0.0 ; ] ;
    lv2:scalePoint [ rdfs:label "on"; rdf:value 1.0 ; ] ;
EOF
		j=$[$j + 1]
		IDX=$[$IDX + 1]
	done
	i=$[$i + 1]
done

echo "]; ."
echo

i=1; while test $i -le $N_INPUTS; do
	echo "matrixmixer:g$i"
	echo ' a param:ControlGroup ;'
	echo ' lv2:name "Source '$i'" ;'
	echo ' lv2:symbol "InGrp'$i'" .'
	i=$[$i + 1]
done

