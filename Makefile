SHELL := /bin/bash
VM_DIR := test/vms
CLOUD_INIT := test/cloud-init
SSH_PORT_LINUX := 2222
VM_KEY := $(VM_DIR)/id_ed25519
SSH_OPTS := -o "StrictHostKeyChecking=no" -o "UserKnownHostsFile=/dev/null" -o "LogLevel=ERROR" -i "$(VM_KEY)"

ARCH_IMAGE_URL := https://geo.mirror.pkgbuild.com/images/latest/Arch-Linux-x86_64-cloudimg.qcow2
ARCH_BASE := $(VM_DIR)/arch-base.qcow2
LINUX_DISK := $(VM_DIR)/linux.qcow2
LINUX_SEED := $(VM_DIR)/seed.img
LINUX_PID := $(VM_DIR)/linux.pid

# --- Linux VM ---

$(VM_DIR):
	mkdir -p $(VM_DIR)

$(VM_KEY): | $(VM_DIR)
	ssh-keygen -t ed25519 -f $@ -N "" -q

$(ARCH_BASE): | $(VM_DIR)
	@echo "Downloading Arch Linux cloud image (~540MB, one-time)..."
	curl -L -o $@ $(ARCH_IMAGE_URL)

$(LINUX_DISK): $(ARCH_BASE)
	cp $(ARCH_BASE) $@
	qemu-img resize $@ 10G

$(LINUX_SEED): $(VM_KEY) $(CLOUD_INIT)/user-data $(CLOUD_INIT)/meta-data | $(VM_DIR)
	@echo "Building cloud-init seed ISO..."
	@# Inject SSH public key into user-data
	@mkdir -p $(VM_DIR)/cloud-init-tmp
	@cp $(CLOUD_INIT)/meta-data $(VM_DIR)/cloud-init-tmp/
	@cat $(CLOUD_INIT)/user-data > $(VM_DIR)/cloud-init-tmp/user-data
	@echo "ssh_authorized_keys:" >> $(VM_DIR)/cloud-init-tmp/user-data
	@echo "  - $$(cat $(VM_KEY).pub)" >> $(VM_DIR)/cloud-init-tmp/user-data
	@mkisofs -output $@ -volid cidata -joliet -rock \
		$(VM_DIR)/cloud-init-tmp/user-data $(VM_DIR)/cloud-init-tmp/meta-data 2>/dev/null || \
	genisoimage -output $@ -volid cidata -joliet -rock \
		$(VM_DIR)/cloud-init-tmp/user-data $(VM_DIR)/cloud-init-tmp/meta-data 2>/dev/null || \
	(hdiutil makehybrid -o $(VM_DIR)/seed -iso -joliet -default-volume-name cidata \
		$(VM_DIR)/cloud-init-tmp/ && mv $(VM_DIR)/seed.iso $@)
	@rm -rf $(VM_DIR)/cloud-init-tmp

.PHONY: linux linux-ssh linux-sync linux-test linux-stop linux-clean linux-purge test help

linux: $(LINUX_DISK) $(LINUX_SEED) $(VM_KEY) ## Start Linux VM
	@if [ -f $(LINUX_PID) ] && kill -0 $$(cat $(LINUX_PID)) 2>/dev/null; then \
		echo "VM already running (pid $$(cat $(LINUX_PID)))"; \
		exit 0; \
	fi
	@echo "Starting Arch Linux x86_64 VM..."
	qemu-system-x86_64 \
		-machine q35 \
		-cpu qemu64 \
		-m 2048 \
		-smp 2 \
		-drive file=$(LINUX_DISK),if=virtio \
		-drive file=$(LINUX_SEED),if=virtio,format=raw \
		-nic user,hostfwd=tcp::$(SSH_PORT_LINUX)-:22 \
		-display none \
		-daemonize \
		-pidfile $(LINUX_PID)
	@echo "Waiting for SSH..."
	@for i in $$(seq 1 180); do \
		ssh -q $(SSH_OPTS) -o "ConnectTimeout=2" -p $(SSH_PORT_LINUX) arch@localhost true 2>/dev/null && break; \
		[ $$i -eq 180 ] && echo "Timed out" && exit 1; \
		sleep 3; \
	done
	@echo "VM ready."

linux-ssh: ## SSH into Linux VM
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost

linux-sync: ## Sync project files into Linux VM (git-tracked files only)
	git ls-files -z | rsync -az --files-from=- --from0 \
		-e "ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX)" \
		. arch@localhost:~/schemulator/

linux-test: linux-sync ## Sync + run tests in Linux VM
	ssh $(SSH_OPTS) -p $(SSH_PORT_LINUX) arch@localhost \
		'cd ~/schemulator && python -m pytest test/ -v'

linux-stop: ## Stop Linux VM
	@if [ -f $(LINUX_PID) ] && kill -0 $$(cat $(LINUX_PID)) 2>/dev/null; then \
		kill $$(cat $(LINUX_PID)); rm -f $(LINUX_PID); echo "VM stopped."; \
	else \
		echo "No VM running."; rm -f $(LINUX_PID); \
	fi

linux-clean: linux-stop ## Delete Linux VM disk (keeps base image)
	rm -f $(LINUX_DISK) $(LINUX_SEED) $(LINUX_PID)

linux-purge: linux-clean ## Delete everything including base image
	rm -f $(ARCH_BASE)
	rm -f $(VM_KEY) $(VM_KEY).pub

# --- Local test ---

test: ## Run tests locally (macOS)
	python -m pytest test/ -v

# --- Help ---

help: ## Show available targets
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2}'

.DEFAULT_GOAL := help
