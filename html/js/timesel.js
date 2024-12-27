
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

class timesel
{
  constructor(ctarget)
  {
    this.ctarget=ctarget;
    this.init();
    ctarget.addEventListener('click', this);
    ctarget.addEventListener('change', this);
    ctarget.addEventListener('input', this);
    ctarget.addEventListener('keypress', this);
    ctarget.addEventListener('wheel', this);
  }
  init()
  {
    let el=this.ctarget;
    try
    {
      var tar=el.getAttribute('data-value').split(":");
    }
    catch
    {
      return false;
    }
    let t=document.createElement('input');
    t.classList.add("time", "sel_hours");
    t.id=`${el.id}_h`;
    t.value=('00'+tar[0]).slice(-2);
    el.appendChild(t);

    let p=document.createElement('div');
    p.classList.add("time", "divider", "no-select");
    p.innerHTML=":";
    el.appendChild(p);

    let m=document.createElement('input');
    m.classList.add("time", "sel_mins");
    m.id=`${el.id}_m`;
    m.value=('00'+tar[1]).slice(-2);
    el.appendChild(m);
  }
  validate_time(event)
  {
    let el=event.target;

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
    if(typeof event.keyCode!=='undefined')
    {
      if(event.keyCode<48||event.keyCode>57)
      {
        return;
      }
      if(tmpStr.length<2)
      {
        tmpStr+=event.key;
      }
      else
      {
        tmpStr=event.key;
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
  onclick(tgt, event)
  {
    //console.log(`event: ${event.type}, ${event.target.id}`);
    //event.target.removeEventListener(event.type, this);
  }
  onwheel(tgt, event)
  {
    const delta=Math.sign(event.deltaY);
    let el=event.target;
    if(el.classList.contains('time'))
    {
      if(el.classList.contains('sel_hours')||el.classList.contains('sel_mins'))
      {
        el.value=String(Number(el.value)-delta);
        tgt.validate_time(event);
      }
    }
  }
  handleEvent(event)
  {
    event.preventDefault();
    let obj=this['on'+event.type];
    if(typeof obj==='function')
    {
      //console.log('Function exists for "'+event.type+'" ('+event.target.id+')');
      obj(this, event);
    }
    else
    {
      this.validate_time(event);
    }
  }
}
init_timesel();
