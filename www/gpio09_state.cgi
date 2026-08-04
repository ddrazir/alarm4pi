#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>GPIO state</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>GPIO 9="
pinctrl get 9
echo " (lo=relay on)</h3>
</body>
</html>"
