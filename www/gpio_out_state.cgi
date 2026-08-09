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
<h3>Output GPIOs:</h3>"
echo "<h3>GPIO 8="
pinctrl get 8
echo " </h3>"
echo "<h3>GPIO 9="
pinctrl get 9
echo " </h3>"
echo "<h3>GPIO 10="
pinctrl get 10
echo " </h3>"
echo "<h3>GPIO 11="
pinctrl get 11
echo " </h3>"
echo "<h3>(lo=relay on)</h3>
</body>
</html>"
