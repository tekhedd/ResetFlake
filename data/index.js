(function(exports) {
    
    // Global config
    var STATS_URL = "stats";
    
    function dhms(seconds)
    {
        // Format seconds as days hours:minutes:seconds
        var days = Math.floor(seconds / 86400);
        
        var hours = Math.floor(seconds / 3600);
        hours = hours % 24;
        
        var minutes = Math.floor(seconds / 60);
        minutes = minutes % 60;
        
        var leftovers = seconds % 60;
        
        var output = "";
        if (days)
        {
            output += days + "d ";
            
            if (hours < 10)
                output += "0";
        }
            
        if (hours)
        {
            output += hours + ":";
            
            if (minutes < 10)
                output += "0";
        }
            
        if (minutes)
        {
            output += minutes + ":";
            if (leftovers < 10)
                leftovers = "0" + leftovers;
        }
            
        output += leftovers;

        if (!days && !hours && !minutes) // only seconds?
        {
            output += 's';
        }
        return output;
    }
    
    function refresh()
    {
        var msg = document.getElementById("msg");
        msg.textContent = "waiting for stats...";
            
        var xhr = new XMLHttpRequest();
        xhr.open('get', STATS_URL, true);
        xhr.responseType = 'json';
        xhr.onload = function()
        {
            var status = xhr.status;
            if (status != 200)
            {
                msg.textContent = xhr.statusText;
                return;
            }
            
            var data = xhr.response;
            msg.textContent = "OK";
            
            // overall status.
            var outage = data.outageDuration;
            
            var statusEl = document.getElementById("status");
            if (outage < 0) // no outage?
            {
                statusEl.textContent = 'OK';
            }
            else
            {
                statusEl.textContent = 'DOWN ' + dhms(outage);
            }
            
            // History
            var lastOutage = data.outageHistory[data.outageHistory.length - 1];
            if (lastOutage.length <= 0) // never!
            {
                document.getElementById("last-outage-was").textContent = "NEVER";
                document.getElementById("last-outage-length").textContent = dhms(lastOutage.length);
            }
            else
            {
                document.getElementById("last-outage-was").textContent = dhms(lastOutage.ago);
                document.getElementById("last-outage-length").textContent = dhms(lastOutage.length);
            }
            
            // Other stats
            
            if (outage < 0) { // no outage?
                document.getElementById("ping").textContent = data.ping;
            }
            else {
                document.getElementById("ping").textContent = "???";
            }
            
            document.getElementById("uptime").textContent = dhms(data.uptime);
            document.getElementById("link-up").textContent = dhms(data.linkUp);
            
            // historical stats
            document.getElementById("reset-count").textContent = data.resetCount;
            document.getElementById("outage-max").textContent = dhms(data.outageMax);
            document.getElementById("outage-avg").textContent = dhms(data.outageAvg);
            document.getElementById("ping-max").textContent = data.pingMax;
            document.getElementById("ping-avg").textContent = data.pingAvg;
        };
        xhr.send();
    }
    
    exports['refresh'] = refresh;

})(this);