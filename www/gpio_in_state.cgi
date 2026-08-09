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
<h3>Input GPIOs:</h3>"
echo "<h3>GPIO 17="
pinctrl get 17
echo " </h3>"
echo "<h3>GPIO 5="
pinctrl get 5
echo " </h3>"
echo "<h3>GPIO 6="
pinctrl get 6
echo " </h3>"
echo "<h3>(hi=alarm event on)</h3>
</body>
</html>"
