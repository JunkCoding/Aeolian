let xhr = new XMLHttpRequest();
var valid_apps = {};
function _(el)
{
  return document.getElementById(el);
}
function openAjaxSpinner()
{
  _("spinner").style.display = 'block';
}
function closeAjaxSpinner()
{
  _("spinner").style.display = 'none';
}
function doCommonXHR(donecallback, successcallback)
{
  xhr.onreadystatechange=function()
  {
    if ( xhr.readyState == 4 )
    {
      if ( xhr.status >= 200 && xhr.status <= 400 )
      {
        var json_response = JSON.parse(xhr.responseText);
        if ( json_response.success == false )
        {
          _("status").innerHTML="<strong style=\"color: red;\">Error: "+xhr.responseText+"</strong>";
          closeAjaxSpinner();
        }
        else
        {
          if ( successcallback && typeof(successcallback) === "function" )
          {
            successcallback(json_response);
          }
        }
      }
      else
      {
        _("status").innerHTML="<strong style=\"color: red;\">Error!  Check connection.  HTTP status: " + xhr.status + "</strong>";
        closeAjaxSpinner();
      }
      if ( donecallback && typeof(donecallback) === "function" )
      {
        donecallback();
      }
    }
  }
}
function doReboot(callback)
{
  _("status").innerHTML="Sending reboot command...";
  openAjaxSpinner();
  xhr.open("GET", "/flash/reboot");
  doCommonXHR(callback, function(json_response)
  {
    _("status").innerHTML="Command sent.  Rebooting!...";
    // todo: use Ajax to test if connection restored before reloading page...
    window.setTimeout(function()
    {
      location.reload(true);
    }, 4000);
  });
  xhr.send();
  return false;
}
function doVerify(partition, callback)
{
  _("status").innerHTML="Verifying App in partition: " + partition + "...";
  openAjaxSpinner();
  xhr.open("GET", "/flash/info?verify=1" + ((partition)?("&partition=" + partition):("")));
  doCommonXHR(callback, function(flinfo)
  {
    let l = flinfo.app.length;
    for(var i=0;i<l;i++)
    {
      if ( typeof flinfo.app[i].valid !== "undefined" )
      {
        valid_apps[flinfo.app[i].name] = flinfo.app[i].valid;
      }
    }
    _("status").innerHTML="Finished verify.  " + partition + " is <b>" + ((valid_apps[partition])?("Valid"):("Not valid")) + "</b>";
    doInfo(function()
    {
      closeAjaxSpinner();
    });
  });
  xhr.send();
  return false;
}
function doSetBoot(partition, callback)
{
  _("status").innerHTML="Setting Boot Flag to partition: " + partition + "...";
  openAjaxSpinner();
  xhr.open("GET", "/flash/setboot" + ((partition)?("?partition=" + partition):("")));
  doCommonXHR(callback, function(json_response)
  {
    doInfo(function()
    {
      _("status").innerHTML="Finished setting Boot Flag to " + partition + ".  <b>Now press Reboot!</b>";
      closeAjaxSpinner();
    });
  });
  xhr.send();
  return false;
}
function doEraseFlash(partition, callback)
{
  _("status").innerHTML="Erasing data in: " + partition + "...";
  openAjaxSpinner();
  xhr.open("GET", "/flash/erase" + ((partition)?("?partition=" + partition):("")));
  doCommonXHR(callback, function(json_response)
  {
    doInfo(function()
    {
      _("status").innerHTML="Finished erasing data in: " + partition + ".  <b>Must reboot to reformat it!</b>";
      closeAjaxSpinner();
    });
  });
  xhr.send();
  return false;
}
function doUpgrade(partition, callback)
{
  var firmware =  document.querySelector('#firmware').files;
  if ( firmware.length == 0 )
  {
    _("status").innerHTML="<strong style=\"color: red;\">No file selected!</strong>";
    return;
  }
  // Only using the first file
  firmware = firmware[0];
  if ( firmware.size == 0 )
  {
    _("status").innerHTML="<strong style=\"color: red;\">File is empty!</strong>";
    return;
  }
  _("status").innerHTML="Uploading App to partition: " + partition;

  _("progressBar").value = 0;
  _("progbar").style.display = 'block';

  // Override the global xhr
  let xhr = new XMLHttpRequest();

  /* Add our handlers */
  xhr.upload.addEventListener("progress", progressHandler, false);
  xhr.addEventListener("load", loadHandler, false);
  xhr.addEventListener("error", errorHandler, false);
  xhr.addEventListener("abort", abortHandler, false);

  xhr.open("POST", "/flash/upload" + ((partition)?("?partition=" + partition):("")));

  //let data = new FormData();
  //data.append('file', firmware);
  //xhr.send(data);
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.send(firmware);
}
function progressHandler(event)
{
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total + " (" + Math.round(percent) + "%)";
}
function loadHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong>Success: Reboot to run new firmware</strong>";
    _("progbar").style.display = 'none';
  });
}
function errorHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong style=\"color: red;\">Error: An unknown error ocurred during upload</strong>";
    _("progbar").style.display = 'none';
  });
}
function abortHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong style=\"color: amber;\">Aborted: Firmware upload aborted</strong>";
    _("progbar").style.display = 'none';
  });
}
function timeoutHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="Upload timed out";
    _("progbar").style.display = 'none';
  });
}
function humanFileSize(B,i)
{
  var e=i?1e3:1024;if(Math.abs(B)<e)return B+" B";
  var a=i?["kB","MB","GB","TB","PB","EB","ZB","YB"]:["KiB","MiB","GiB","TiB","PiB","EiB","ZiB","YiB"],t=-1;
  do B/=e,++t;while(Math.abs(B)>=e&&t<a.length-1);
  return B.toFixed(3)+" "+a[t]
}
function doInfo(callback)
{
  xhr.open("GET", "/flash/info");
  doCommonXHR(callback, function(flinfo)
  {
    var dataformats = {0: "ota_data", 1: "phy_init", 2: "nvs", 3: "coredump", 0x81: "fat", 0x82: "spiffs"};

    let l = flinfo.app.length;
    let ntbdy = document.createElement('tbody');
    for(var i=0;i<l;i++)
    {
      if (typeof flinfo.app[i].valid !== "undefined")
      {
        valid_apps[flinfo.app[i].name] = flinfo.app[i].valid;
      }
      var tr = "<tr>"
                + "<td>" + flinfo.app[i].name + "</td>"
                + "<td>" + humanFileSize(flinfo.app[i].size,0) + "</td>"
                + "<td>" + flinfo.app[i].version + "</td>"
                + "<td>" + ((valid_apps[flinfo.app[i].name])?("&#10004;"):(('<input type="submit" value="Verify ' + flinfo.app[i].name + '!" onclick="doVerify( \'' + flinfo.app[i].name + '\')" />'))) + "</td>"
                + "<td>" + ((flinfo.app[i].running)?("&#10004;"):("")) + "</td>"
                + '<td>' + ((flinfo.app[i].bootset)?("&#10004;"):('<input type="submit" value="Set Boot ' + flinfo.app[i].name + '!" onclick="doSetBoot( \'' + flinfo.app[i].name + '\')" />')) + '</td>'
                + '<td>' + (((flinfo.app[i].ota) && !(flinfo.app[i].running))?('<input type="submit" value="Upload to ' + flinfo.app[i].name + '!" onclick="doUpgrade( \'' + flinfo.app[i].name + '\')" />'):('')) + '</td>'
                + "</tr>";
      var nr = ntbdy.insertRow();
      nr.innerHTML = tr;
    };
    let otbdy = document.querySelector("#tblApp > tbody");
    otbdy.parentNode.replaceChild(ntbdy, otbdy);

    l = flinfo.data.length;
    ntbdy = document.createElement('tbody');
    for(var i=0;i<l;i++)
    {
      var tr = "<tr>"
                + "<td>" + flinfo.data[i].name + "</td>"
                + "<td>" + humanFileSize(flinfo.data[i].size,0) + "</td>"
                + "<td>" + dataformats[flinfo.data[i].format] + "</td>"
                + '<td>' + ('<input type="submit" value="Erase ' + flinfo.data[i].name + '!" onclick="doEraseFlash( \'' + flinfo.data[i].name + '\')" />') + '</td>'
                + "</tr>";
      var nr = ntbdy.insertRow();
      nr.innerHTML = tr;
    };
    otbdy = document.querySelector("#tblData > tbody");
    otbdy.parentNode.replaceChild(ntbdy, otbdy);
  });
  xhr.send();
  return false;
}
function page_onload()
{
  openAjaxSpinner();
  doInfo(function()
  {
    closeAjaxSpinner();
    _("status").innerHTML="Ready.";
  });
}
