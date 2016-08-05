onLoad(function() {
  bnd($("#cback-form"), "submit", setCBack); 
  fetchIcon();
  fetchCBack();
});

// disable START button, enable STOP button, show spinner
function scan_start()
{
  var cb = $("#scan-start");
  addClass(cb, "pure-button-disabled");
  cb.setAttribute("disabled", "");
  var cb = $("#scan-stop");
  removeClass(cb, "pure-button-disabled");
  cb.removeAttribute("disabled");
  $("#scan-spinner").removeAttribute("hidden");
  $("#progressbar").style.width="0%";
  $("#dev-count").innerHTML = '0 %';
  $("#progress").removeAttribute("hidden");
}

// enable START button, disable STOP button, hide spinner
function scan_stop()
{
  var cb = $("#scan-start");
  removeClass(cb, "pure-button-disabled");
  cb.removeAttribute("disabled");
  var cb = $("#scan-stop");
  addClass(cb, "pure-button-disabled");
  cb.setAttribute("disabled", "true");
  $("#scan-spinner").setAttribute("hidden","true");
  $("#progress").setAttribute("hidden","true");
}

// A = HTML element for start address
// B = HTML element for final address
function startScan(a,b) 
{
  _from = parseInt(a.value,10);
  if(isNaN(_from) || _from < 0) _from = 0;
  _to = parseInt(b.value,10);
  if(isNaN(_to)) _to = 50;
  if(_to > 254) _to = 254;
  if(_from > _to) _from = _to;
  a.value = _from;
  b.value = _to;

  hideWarning();
  scan_start();

  ajaxReq("GET", "/ctl/scan_start?start="+_from+"&final="+_to, function(resp) {
    window.setTimeout(scanProgress, 1000);
	}, function(stat,resp) {
	  scan_stop();
		showWarning("ERROR - "+resp);
	}); 
}

function stopScan()
{
  ajaxReq("GET", "/ctl/scan_stop", function(resp) {
    scan_stop();
    window.setTimeout(fetchIcon, 100);
	}, function(stat,resp) {
    scan_stop();
		showWarning("ERROR - "+resp);
	}); 
}

function displayProgress(data)
{
  var proc = 0, el = $("#progress");
  if(el != null)
  {
    proc = Math.round((data.current_address - _from)*100/(1 + _to - _from));
    $("#dev-count").innerHTML = data.device_count + ' devices = (' 
      + proc + ' %)';
    $("#progressbar").style.width = proc+'%';
  }
  if(data.in_progress)
    window.setTimeout(scanProgress, 1000);
  else 
  {
    scan_stop();
    window.setTimeout(fetchIcon, 100);
  }
}

function scanProgress()
{
  ajaxJson("GET", "/ctl/scan_status", displayProgress, function () {
    prcTimeout = window.setTimeout(scanProgress, 1000);
  });
}

function displayIcon(data) {
  el = $("#list-table");
  if (el != null) 
  {
    var x = '';
    for(k in data)
      x += '<tr><td>Device #' + data[k].address + '</td></tr>';
    el.innerHTML = x;
  }

  $("#list-spinner").setAttribute("hidden", "");
}

function fetchIcon() {
  ajaxJson("GET", "/ctl/device_list", displayIcon, function () {
    window.setTimeout(fetchIcon, 1000);
  });
}

function displayCBack(data) {
  Object.keys(data).forEach(function (v) {
    el = $("#" + v);
    if (el != null) {
      if (el.nodeName === "INPUT") el.value = data[v];
      else el.innerHTML = data[v];
      return;
    }

    el = document.querySelector('input[name="' + v + '"]');
    if (el == null)
      el = document.querySelector('select[name="' + v + '"]');

    if (el != null) {
      if (el.type == "checkbox") {
        el.checked = data[v] == "enabled";
      } else el.value = data[v];
    }
  });

  $("#cback-spinner").setAttribute("hidden", "");
  $("#cback-form").removeAttribute("hidden"); 
}

function fetchCBack() {
  ajaxJson("GET", "/ctl/callbacks", displayCBack, function () {
    window.setTimeout(fetchCBack, 1000);
  });
}

function setCBack(e) {
  e.preventDefault();
  var url = "/ctl/callbacks?1=1";
  var i, inputs = document.querySelectorAll("#" + e.target.id + " input,select");
  for (i = 0; i < inputs.length; i++) 
  {
    if (inputs[i].type == "checkbox") 
    {
      var val = (inputs[i].checked) ? 1 : 0;
      url += "&" + inputs[i].name + "=" + val;
    }
    else url += "&" + inputs[i].name + "=" + inputs[i].value;
  };

  hideWarning();
  var n = e.target.id.replace("-form", "");
  var cb = $("#" + n + "-button");
  addClass(cb, "pure-button-disabled");
  ajaxSpin("POST", url, function (resp) {
    showNotification("Settings updated");
    removeClass(cb, "pure-button-disabled");
  }, function (s, st) {
    showWarning("Error: " + st);
    removeClass(cb, "pure-button-disabled");
    window.setTimeout(fetchCBack, 100);
  });
} 