#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Alarm state</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Alarm state:"
pinctrl get 18
echo "</h3>
<h3>(pin=lo -> armed)</h3>
</body>
</html>"
