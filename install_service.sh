#!/bin/bash
echo "Installing a systemd service to automatically start alarm4pi after booting."

echo "[Unit]
Description=alarm4pi service
After=network-online.target
Wants=network-online.target

[Service]
Type=forking
User=pi
ExecStart=`pwd`/alarm4pid

[Install]
WantedBy=multi-user.target
" > /etc/systemd/system/alarm4pi.service

echo "Enabling the service."
systemctl enable alarm4pi
