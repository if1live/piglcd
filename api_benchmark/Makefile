SHELL_SRC	= shell.sh
RPI_GPIO_SRC	= rpi_gpio.py
PYTHON_WIRINGPI2_SRC = python_wiringpi2.py
C_WIRINGPI_SRC = c_wiringpi.c

all: c_wiringpi rpi_gpio python_wiringpi2 shell

shell:
	@echo "bash loop count : 1000"
	bash -c "time ./$(SHELL_SRC) 1000"

	@echo "bash loop count : 10000"
	bash -c "time ./$(SHELL_SRC) 10000"

	@# too slow, skip 100000
	@#@echo "bash loop count : 100000"
	@#bash -c "time ./$(SHELL_SRC) 100000"

rpi_gpio:
	@echo "RPi.GPIO loop count : 1000"
	sudo -E bash -c "time python $(RPI_GPIO_SRC) 1000"

	@echo "RPi.GPIO loop count : 10000"
	sudo -E bash -c "time python $(RPI_GPIO_SRC) 10000"

	@echo "RPi.GPIO loop count : 100000"
	sudo -E bash -c "time python $(RPI_GPIO_SRC) 100000"

python_wiringpi2:
	@echo "wiringpi2 loop count : 1000"
	sudo -E bash -c ". ./.venv/bin/activate && time python $(PYTHON_WIRINGPI2_SRC) 1000"

	@echo "wiringpi2 loop count : 10000"
	sudo -E bash -c ". ./.venv/bin/activate && time python $(PYTHON_WIRINGPI2_SRC) 10000"

	@echo "wiringpi2 loop count : 100000"
	sudo -E bash -c ". ./.venv/bin/activate && time python $(PYTHON_WIRINGPI2_SRC) 100000"

c_wiringpi:
	clang $(C_WIRINGPI_SRC) -lwiringPi

	@echo "c wiring pi loop count : 1000"
	sudo -E bash -c "time ./a.out 1000"

	@echo "c wiring pi loop count : 10000"
	sudo -E bash -c "time ./a.out 10000"

	@echo "c wiring pi loop count : 100000"
	sudo -E bash -c "time ./a.out 100000"

clean:
	rm -rf a.out
