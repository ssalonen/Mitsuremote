#!/usr/bin/env bash
set -e
script_dir=$(dirname $0)
TMPFILE=$(mktemp -t  esp_stacktrace.XXXXXXXX)

[ ! -f $script_dir/EspStackTraceDecoder.jar ] && wget https://github.com/littleyoda/EspStackTraceDecoder/releases/download/untagged-59a763238a6cedfe0362/EspStackTraceDecoder.jar -O $script_dir/EspStackTraceDecoder.jar

echo "Copy paste stack trace. Should end with <<<stack<<<"

function onexit {
	rm $TMPFILE
}
trap onexit EXIT


while read line
do
  # break if the line is empty
  [[ "$line" =~ "<<<stack<<<" ]] && break
  echo "  $line" >> $TMPFILE
done



java -jar ${script_dir}/EspStackTraceDecoder.jar ~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-addr2line .pio/build/wemos_d1/firmware.elf $TMPFILE
