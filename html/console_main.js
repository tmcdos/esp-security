	console_url = "/console/text";

  onLoad(function() {
    fetchText(100, true);

    $("#bridge-button").addEventListener("click", function(e) {
      e.preventDefault();
			var co = $("#console");
      co.innerHTML = "";
      ajaxSpin('POST', "/console/port?port="+document.bridgeform.port.value,
        function(resp) { showNotification("Bridge port saved"); co.textEnd = 0; },
        function(s, st) { showWarning("Error changing TCP bridge port"); }
      );
    });

    $("#clear-button").addEventListener("click", function(e) {
        e.preventDefault();
        var co = $("#console");
        co.innerHTML = "";
    });

    ajaxJson('GET', "/console/baud",
      function(data) { $("#baud-sel").value = data.rate; $("#tcp_port").value = data.port; },
      function(s, st) { showNotification(st); }
    );

    bnd($("#baud-sel"), "change", function(ev) {
      ev.preventDefault();
      var baud = $("#baud-sel").value;
      if(baud !=0)
        ajaxSpin('POST', "/console/baud?rate="+baud,
          function(resp) { showNotification("" + baud + " baud set"); },
          function(s, st) { showWarning("Error setting baud rate: " + st); }
        );
    });

    consoleSendInit();

    addClass($('html')[0], "height100");
    addClass($('body')[0], "height100");
    addClass($('#layout'), "height100");
    addClass($('#layout'), "flex-vbox");
  });
