#!/bin/bash
echo "HTTP/1.0 200 OK
Content-type: text/html
Connection: close
Cache-Control: no-store
"
echo "<!DOCTYPE html><html><head>
<title>Log</title>
<meta name="viewport" content="initial-scale=1"></head>
<body>
<h3>alarm4pi tunneling log:</h3>
<pre><xmp>"
#cat `eval echo "~$USER"`/.socketxp/socketxp.log
cat "$HOME"/.socketxp/socketxp.log
journalctl -n 30 -u socketxp.service
echo "</xmp></pre>
</body>
</html>"
