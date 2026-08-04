#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>GPIO low</title>
<meta name="viewport" content="width=device-width, initial-scale=1"></head>
<body>
<h3>Setting GPIO 11 to low...</h3>
<pre>"
pinctrl set 11 op dl
echo "</pre>
</body>
</html>"
