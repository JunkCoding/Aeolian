
clklst=[];
clkgrp=[];

/* =========================================================== */
function init_timesel()
{
  // Remove any existing class associations
  // ------------------------------------------------
  let g=clkgrp.length;
  if(g>0)
  {
    for(let i=0; i<g; i++)
    {
      let oldNode=document.getElementById(clkgrp[i].id);
      let newNode=oldNode.cloneNode(true);
      oldNode.parentNode.insertBefore(newNode, oldNode);
      oldNode.parentNode.removeChild(oldNode);
      clkgrp[i]=undefined;
    }
    clkgrp=[];
  }

  clklist=document.getElementsByClassName("timesel");
  let l=clklist.length;
  for(let i=0; i<l; i++)
  {
    clklist[i]=new timesel(clklist[i]);
  }
}

function append_timesel(el)
{
  let l=clkgrp.length;
  for(let i=0; i<l; i++)
  {
    if(clkgrp[i].id===el.id)
    {
      return;
    }
  }
  clkgrp[l]=new timesel(el);
}

function timesel(el)
{
  this.mousedown=false;
  this.hasMoved=false;
  this.cur_tgt=null;
  this.lst_tgt=null;
  this.ovr_tgt=null;
  this.clkgrp=el;
  this.val=el.getAttribute('data-value');
  this.id=el.id;
  this.init();
}

/* =========================================================== */
timesel.prototype={
  init: function()
  {
    var obj=this;
    let tar=this.val.split(":");

    let t=document.createElement('input');
    t.classList.add("time", "sel_hours");
    t.id=`${this.id}_h`;
    t.value=('00'+tar[0]).slice(-2);
    this.clkgrp.appendChild(t);

    let p=document.createElement('div');
    p.classList.add("time", "divider", "no-select");
    p.innerHTML=":";
    this.clkgrp.appendChild(p);

    let m=document.createElement('input');
    m.classList.add("time", "sel_mins");
    m.id=`${this.id}_m`;
    m.value=('00'+tar[1]).slice(-2);
    this.clkgrp.appendChild(m);

    obj.clkgrp.onmousedown=function(e)
    {
      let el=e.target;
      console.log("mousedown: "+el.id);
      if(el.classList.contains("sel_hours")||el.classList.contains("sel_mins"))
      {
        el.classList.add("clock-wrapper");
        let div=document.createElement('div');
        div.id="pdiv";
        div.classList.add("clock");
        el.appendChild(div);
      }
    };
  },
  getValue: function()
  {
    return this.val;
  }
};


/**** **** **** **** **** **** **** ****/
function validate_time(e)
{
  e.preventDefault();

  let el=e.target;

  /* Stotr the current value without leading zeros */
  let tmpStr=el.value.replace(/\b0+/g, '');

  /* Clear the string, so we can just return on invalid input */
  el.value="00";

  /* Are we actually interested in this element? */
  if(!el.classList.contains("time"))
  {
    return;
  }

  // Check content is numeric */
  let isNum=tmpStr.match(/^[0-9]+$/)!=null;

  /* Check for keypress */
  if(typeof e.keyCode!=='undefined')
  {
    if(e.keyCode<48||e.keyCode>57)
    {
      return;
    }
    if(tmpStr.length<2)
    {
      tmpStr+=e.key;
    }
    else
    {
      tmpStr=e.key;
    }
  }

  /* This works for stuff that's pasted */
  if(tmpStr.length>2)
  {
    return;
  }

  let tmpNum=Number(tmpStr);
  if(tmpNum<0)
  {
    return;
  }

  /* Deal with individual needs of hours/minutes */
  if(el.classList.contains("sel_hours"))
  {
    if(tmpNum>23)
    {
      el.value="23";
      return;
    }
  }
  else if(el.classList.contains("sel_mins"))
  {
    if(tmpNum>59)
    {
      el.value="59";
      return;
    }
  }

  /* Return with whatever, it seems okay */
  el.value=('00'+tmpStr).slice(-2);
}
window.addEventListener("wheel", event =>
{
  const delta=Math.sign(event.deltaY);
  let el=event.target;
  if(el.classList.contains('time'))
  {
    if(el.classList.contains('sel_hours')||el.classList.contains('sel_mins'))
    {
      el.value=String(Number(el.value)-delta);
      validate_time(event);
    }
  }
});
window.addEventListener("change", function(event)
{
  validate_time(event);
});
window.addEventListener("input", function(event)
{
  validate_time(event);
});
window.addEventListener("keypress", function(event)
{
  validate_time(event);
});

