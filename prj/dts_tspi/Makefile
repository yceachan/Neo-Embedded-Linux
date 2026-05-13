REPO_BASE=/home/pi
.PHONY: all kernel

all: kernel

kernel:
	bash -c "source $(REPO_BASE)/imx/tspi_env.sh && cd \$$SDK_DIR && ./build.sh kernel"
