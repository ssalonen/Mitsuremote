.PHONY: build
build:
	platformio run --environment wemos_d1

.PHONY: deploy
deploy: build
	scp .pio/build/wemos_d1/firmware.bin pi@192.168.1.101:wemos_d1_firmware.bin
	ssh pi@192.168.1.101 '[ ! -f espota.py ]' \
		&& scp ~/.platformio/packages/framework-arduinoespressif8266/tools/espota.py pi@192.168.1.101: || :
	ssh pi@192.168.1.101 python3 espota.py \
		--ip=192.168.1.167 \
		--host_ip=192.168.1.101 \
		--port=8266 \
		--host_port=10001 \
		--file=wemos_d1_firmware.bin \
		--debug \
		--progress
