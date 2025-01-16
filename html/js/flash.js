/* jshint esversion: 8 */
"use strict";


let xhr=new XMLHttpRequest();
let valid_apps = {};
function doCommonXHR(donecallback, successcallback)
{
  xhr.onreadystatechange=function()
  {
    if ( xhr.readyState == 4 )
    {
      if ( xhr.status >= 200 && xhr.status <= 400 )
      {
        let json_response = JSON.parse(xhr.responseText);
        if ( json_response.noerr == false )
        {
          _("status").innerHTML="<strong style=\"color: red;\">Error: "+xhr.responseText+"</strong>";
          closeBusyMesg();
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
        closeBusyMesg();
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
  openBusyMesg("Rebooting...");
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
  openBusyMesg("Verifying");
  xhr.open("GET", "/flash/info?verify=1" + ((partition)?("&partition=" + partition):("")));
  doCommonXHR(callback, function(flinfo)
  {
    let l = flinfo.app.length;
    for(let i=0;i<l;i++)
    {
      if ( typeof flinfo.app[i].valid !== "undefined" )
      {
        valid_apps[flinfo.app[i].name] = flinfo.app[i].valid;
      }
    }
    _("status").innerHTML="Finished verify.  " + partition + " is <b>" + ((valid_apps[partition])?("Valid"):("Not valid")) + "</b>";
    doInfo(function()
    {
      closeBusyMesg();
    });
  });
  xhr.send();
  return false;
}
function doSetBoot(partition, callback)
{
  _("status").innerHTML="Setting Boot Flag to partition: " + partition + "...";
  openBusyMesg("Setting...");
  xhr.open("GET", "/flash/setboot" + ((partition)?("?partition=" + partition):("")));
  doCommonXHR(callback, function(json_response)
  {
    doInfo(function()
    {
      _("status").innerHTML="Finished setting Boot Flag to " + partition + ".  <b>Now press Reboot!</b>";
      closeBusyMesg();
    });
  });
  xhr.send();
  return false;
}
function doEraseFlash(partition, callback)
{
  _("status").innerHTML="Erasing data in: " + partition + "...";
  openBusyMesg("Erasing...");
  xhr.open("GET", "/flash/erase" + ((partition)?("?partition=" + partition):("")));
  doCommonXHR(callback, function(json_response)
  {
    doInfo(function()
    {
      _("status").innerHTML=`Finished erasing data in "${partition}".`;
      closeBusyMesg();
    });
  });
  xhr.send();
  return false;
}
function doUpgrade(partition, callback)
{
  let firmware = document.getElementById("sel-fw");
  if ( firmware.files.length == 0 )
  {
    _("status").innerHTML="<strong style=\"color: red;\">No file selected!</strong>";
    return;
  }
  // Only using the first file
  firmware = firmware.files[0];
  if ( firmware.size == 0 )
  {
    _("status").innerHTML="<strong style=\"color: red;\">File is empty!</strong>";
    return;
  }
  _("status").innerHTML="Uploading App to partition: " + partition;

  _("progressBar").value = 0;
  _("progbar").style.display = "block";
  _("main").style.pointerEvents = "none";

  // Override the global xhr
  let xhr = new XMLHttpRequest();

  /* Add our handlers */
  xhr.upload.addEventListener("progress", progressHandler, false);
  xhr.addEventListener("load", loadHandler, false);
  xhr.addEventListener("error", errorHandler, false);
  xhr.addEventListener("abort", abortHandler, false);

  xhr.open("POST", "/flash/upload" + ((partition)?("?partition=" + partition):("")));

  //let data = new FormData();
  //data.append("file", firmware);
  //xhr.send(data);
  xhr.setRequestHeader("Content-Type", "application/octet-stream");
  xhr.send(firmware);
}
function progressHandler(event)
{
  let percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total + " (" + Math.round(percent) + "%)";
}
function loadHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong>Success: Reboot to run new firmware</strong>";
    _("progbar").style.display = "none";
    _("main").style.pointerEvents = "";
  });
}
function errorHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong style=\"color: red;\">Error: An unknown error ocurred during upload</strong>";
    _("progbar").style.display = "none";
    _("main").style.pointerEvents = "";
  });
}
function abortHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="<strong style=\"color: amber;\">Aborted: Firmware upload aborted</strong>";
    _("progbar").style.display = "none";
    _("main").style.pointerEvents = "";
  });
}
function timeoutHandler(event)
{
  doInfo(function()
  {
    /*_("status").innerHTML = event.target.responseText;*/
    _("status").innerHTML="Upload timed out";
    _("progbar").style.display = "none";
    _("main").style.pointerEvents = "";
  });
}
function humanFileSize(B,i)
{
  let e=i?1e3:1024;if(Math.abs(B)<e)return B+" B";
  let a=i?["kB","MB","GB","TB","PB","EB","ZB","YB"]:["KiB","MiB","GiB","TiB","PiB","EiB","ZiB","YiB"],t=-1;
  do B/=e,++t;while(Math.abs(B)>=e&&t<a.length-1);
  return B.toFixed(3)+" "+a[t]
}
function doInfo(callback)
{
  xhr.open("GET", "/flash/info");
  doCommonXHR(callback, function(flinfo)
  {
    let dataformats = {0: "ota_data", 1: "phy_init", 2: "nvs", 3: "coredump", 0x81: "fat", 0x82: "spiffs"};

    let l = flinfo.app.length;
    let ntbdy = document.createElement("tbody");
    for(let i=0;i<l;i++)
    {
      if (typeof flinfo.app[i].valid !== "undefined")
      {
        valid_apps[flinfo.app[i].name] = flinfo.app[i].valid;
      }
      let tr = "<tr>"
                + "<td>" + flinfo.app[i].name + "</td>"
                + "<td>" + humanFileSize(flinfo.app[i].size,0) + "</td>"
                + "<td>" + flinfo.app[i].version + "</td>"
                + "<td>" + ((valid_apps[flinfo.app[i].name])?("&#10004;"):(('<input type="submit" class="verify" value="Verify ' + flinfo.app[i].name + '!" onclick="doVerify( \'' + flinfo.app[i].name + '\')" />'))) + "</td>"
                + "<td>" + ((flinfo.app[i].running)?("&#10004;"):("")) + "</td>"
                + '<td>' + ((flinfo.app[i].bootset)?("&#10004;"):('<input type="submit" class="setboot" value="Set Boot ' + flinfo.app[i].name + '!" onclick="doSetBoot( \'' + flinfo.app[i].name + '\')" />')) + '</td>'
                + '<td>' + (((flinfo.app[i].ota) && !(flinfo.app[i].running))?('<input type="submit" class="upload" value="Upload to ' + flinfo.app[i].name + '!" onclick="doUpgrade(\'' + flinfo.app[i].name + '\')" disabled />'):('')) + '</td>'
                + "</tr>";
      let nr = ntbdy.insertRow();
      nr.innerHTML = tr;
    };
    let otbdy = document.querySelector("#tblApp > tbody");
    otbdy.parentNode.replaceChild(ntbdy, otbdy);

    l = flinfo.data.length;
    ntbdy = document.createElement("tbody");
    for(let i=0;i<l;i++)
    {
      let tr = "<tr>"
                + `<td>${flinfo.data[i].name}</td>`
                + `<td>${humanFileSize(flinfo.data[i].size,0)}</td>`
                + `<td>${dataformats[flinfo.data[i].format]}</td>`
                + `<td><input type="submit" class="erase" value="Erase ${flinfo.data[i].name}!" onclick="doEraseFlash('${flinfo.data[i].name}')" /></td>`
                + "</tr>";
      let nr = ntbdy.insertRow();
      nr.innerHTML = tr;
    };
    otbdy = document.querySelector("#tblData > tbody");
    otbdy.parentNode.replaceChild(ntbdy, otbdy);
  });
  xhr.send();
  return false;
}
function enable_fw_upload(set_enabled)
{
  let el = document.getElementsByClassName("upload");
  for(let i=0;i < el.length;i ++)
  {
    el[i].disabled = !set_enabled;
  }
}
function page_onload()
{
  set_background();
  openBusyMesg("Loading...");
  doInfo(function()
  {
    closeBusyMesg();
    _("status").innerHTML="Ready.";
  });

  let sel_fw = document.getElementById("sel-fw");
  let dis_fw = document.getElementById("fw-details");

  sel_fw.addEventListener("change", () => {
    if ( sel_fw.files.length > 0 )
    {
      let firmware = sel_fw.files[0];
      if ( firmware.type != "application/octet-stream" )
      {
        enable_fw_upload(false);
        _("status").innerHTML="<strong style=\"color: red;\">Invalid file</strong>";
        dis_fw.innerHTML = "No firmware selected";
        sel_fw.value = null;
      }
      else
      {
        let fileSize = (firmware.size / 1024).toFixed(1);
        let suffix = "KB";
        if ( fileSize >= 1024 )
        {
          fileSize = (fileSize / 1024).toFixed(1);
          suffix = "MB";
        }
        enable_fw_upload(true);
        dis_fw.innerHTML = `<p>${firmware.name}</p>&nbsp;<p>(${fileSize}${suffix})</p>`;
        _("status").innerHTML="Ready.";
      }
    }
    else
    {
      enable_fw_upload(false);
      dis_fw.innerHTML = "No firmware selected";
    }
  });
}
