#!/bin/bash
echo "Uninstalling a systemd service to automatically start alarm4pi after booting."

systemctl disable alarm4pi

rm /etc/systemd/system/alarm4pi.service


echo "Uninstalling a systemd service to automatically start socketxp tunneling after booting."

# systemctl disable socketxp.service

socketxp service uninstall

