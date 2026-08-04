#!/bin/bash
echo "Installing a systemd service to automatically start alarm4pi after booting."

USER_NAME="${SUDO_USER:-$(id -un)}"

cat > /etc/systemd/system/alarm4pi.service <<EOF
[Unit]
Description=alarm4pi service
After=network-online.target
Wants=network-online.target

[Service]
Type=forking
User=$USER_NAME
ExecStartPre=/bin/sh -c 'until ping -c 1 8.8.8.8; do sleep 1; done;'
ExecStart=/home/$USER_NAME/Desktop/alarm4pi/alarm4pid

[Install]
WantedBy=multi-user.target
EOF

echo "Enabling the service."
systemctl enable alarm4pi

exit 0 # Comment this line (and set the authtoken below?) if you prefer that the tunneling service is started at boot

echo "Installing a systemd service to automatically start socketxp tunneling after booting."

echo "{
    \"authtoken\": \"\",
    \"tunnel_enabled\": true,
    \"tunnels\" : [{
        \"destination\": \"http://localhost:8008\",
        \"protocol\": \"http\",
        \"custom_domain\": \"\"
    }],
    \"relay_enabled\": false,
    \"relays\": [{
    \"destination\": \"http://localhost:8008\"
    }]
}" > socketxp_config.json

socketxp service install --config $(pwd)/socketxp_config.json

#echo "Enabling the service."
#systemctl enable socketxp.service
