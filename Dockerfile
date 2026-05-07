# Pinned image digest (resolved 2026-05): swap when bumping Arch baseline.
# Using a digest instead of `latest` so a pacman repo update can't silently
# break CI between PRs (critic finding #38).
FROM --platform=linux/amd64 archlinux:base-20260101.0.290464
# DisableSandbox: required for x86_64 emulation on ARM hosts (seccomp not supported)
# SigLevel kept at the default (Required DatabaseOptional) — pacman-key trust
# chain is initialised in the base image. Don't disable signature verification;
# any compromised mirror could otherwise inject a backdoor (critic finding #39).
RUN echo 'DisableSandbox' >> /etc/pacman.conf && \
    pacman -Syu --noconfirm python python-pycryptodome python-pytest git && \
    pacman -Scc --noconfirm
WORKDIR /schemulator
