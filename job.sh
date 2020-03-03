#!/bin/bash
CMD="scratch/d2tcp-test --flow_monitor=true"
while getopts ":f:" opt; do
    case $opt in
        f)
            CMD=$OPTARG
            ;;
        ?)
            echo "Invalid option"
            ;;
    esac
done
echo "CMD='$CMD'"
./waf --run "$CMD" && python2 flowmon-parse-results.py trace_tmprm.xml
