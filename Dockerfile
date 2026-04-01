FROM --platform=linux/amd64 archlinux:latest
RUN sed -i 's/^SigLevel.*/SigLevel = Never/' /etc/pacman.conf && \
    sed -i 's/^#DisableSandbox/DisableSandbox/' /etc/pacman.conf && \
    echo 'DisableSandbox' >> /etc/pacman.conf && \
    pacman -Syu --noconfirm python python-pycryptodome python-pytest git && \
    pacman -Scc --noconfirm
WORKDIR /schemulator
