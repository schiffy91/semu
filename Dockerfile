FROM --platform=linux/amd64 archlinux:latest
# DisableSandbox: required for x86_64 emulation on ARM hosts (seccomp not supported)
# SigLevel=Never: acceptable for CI test containers only
RUN echo 'DisableSandbox' >> /etc/pacman.conf && \
    sed -i 's/^SigLevel.*/SigLevel = Never/' /etc/pacman.conf && \
    pacman -Syu --noconfirm base-devel bash git && \
    pacman -Scc --noconfirm
WORKDIR /semu
