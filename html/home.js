onLoad(function() {
  makeAjaxInput("system", "name");
  fetchPins();
  fetchRele();
  getWifiInfo();
  getSystemInfo();
  bnd($("#pinform"), "submit", setPins);
});
