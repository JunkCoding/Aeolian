


function getOffset(el)
{
  var _x=0;
  var _y=0;
  while(el&&!isNaN(el.offsetLeft)&&!isNaN(el.offsetTop))
  {
    _x+=el.offsetLeft-el.scrollLeft;
    _y+=el.offsetTop-el.scrollTop;
    el=el.offsetParent;
  }
  return {top: _y, left: _x};
}
class clockPicker
{
  /* Constants (will fix) */
  static #height=180;
  static #width=180;
  /* Outer radius of minutes/hours */
  static #outerRadius=73;
  /* Inner radius of hours */
  static #innerRadius=50;
  /* Radius of number segment */
  static #offsetToNumCenter=14;
  /* Index of picker arrays */
  static SEL_HOURS=0;
  static SEL_MINS=1;

  /* Shared */
  /**
   * @property {array|undefined} items
   */
  static #pin=undefined;
  static #interface=[
    {items: undefined, disp: undefined, ovr_tgt: undefined, sel_tgt: undefined, ini_tgt: undefined},
    {items: undefined, disp: undefined, ovr_tgt: undefined, sel_tgt: undefined, ini_tgt: undefined}];
  static #hands=[undefined, undefined];
  /**
   * @type {HTMLDivElement}
   */
  static #parentdiv;

  #numOffset;
  #parentRect;
  #offsetToParentCenter;
  #centre;

  #mousedown;
  #hasMoved;
  /* Previous target we had the mouse over */
  #pre_tgt;

  #picker;
  #curInt;

  /* =========================================================== */
  /**
   *
   * @param {HTMLDivElement} pElement
   */
  constructor(pElement)
  {
    /* Object manipulation */
    this.#mousedown=false;
    this.#hasMoved=false;
    this.#pre_tgt=undefined;

    /* Display coordination */
    this.#picker=clockPicker.SEL_HOURS;

    /* default */
    this.#curInt=clockPicker.#interface[this.#picker];


    /* Exposed to the end user */
    this.time={hours: "00", mins: "00"};

    /* Create the parent div on first invocation. */
    if(clockPicker.#parentdiv==undefined)
    {
      clockPicker.#parentdiv=document.createElement("div");
      clockPicker.#parentdiv.classList.add("clock", "clock-wrapper");
      clockPicker.#parentdiv.style.width=`${clockPicker.#width}px`;
      clockPicker.#parentdiv.style.height=`${clockPicker.#height}px`;

      /* Configure some basics for later utility */
      this.#parentRect=clockPicker.#parentdiv.getBoundingClientRect();
      if(this.#parentRect.width>0)
      {
        this.#offsetToParentCenter=Math.round(clockPicker.#parentdiv.offsetWidth/2);
        this.#centre={x: this.#parentRect.left+((this.#parentRect.width-0)/2), y: this.#parentRect.top+((this.#parentRect.height-0)/2)};
      }
      else
      {
        this.#offsetToParentCenter=Math.round(clockPicker.#width/2);
        this.#centre={x: Math.round(clockPicker.#height/2), y: Math.round(clockPicker.#width/2)};
      }

      this.#numOffset=this.#offsetToParentCenter-clockPicker.#offsetToNumCenter;

      /* Add user interaction (we only need to do this once) */
      clockPicker.#parentdiv.addEventListener("click", this);
      clockPicker.#parentdiv.addEventListener("dblclick", this);
      clockPicker.#parentdiv.addEventListener("mousemove", this);
      clockPicker.#parentdiv.addEventListener("mousedown", this);
      clockPicker.#parentdiv.addEventListener("mouseup", this);
      clockPicker.#parentdiv.addEventListener("mouseleave", this);
    }

    /* Check if we are being added to an element */
    if(pElement!=undefined)
    {
      pElement.appendChild(clockPicker.#parentdiv);
    }
    //console.log(this.#offsetToParentCenter, this.#centre.x, this.#centre.y, this.#numOffset);

    /* Create the centre pin */
    if(clockPicker.#pin==undefined)
    {
      this.create_centre_pin();
    }

    /* Create the focus line */
    if(clockPicker.#hands[0]==undefined)
    {
      console.log("creating foc_line");
      this.foc_line_create();
    }

    /* Create the selection line */
    if(clockPicker.#hands[1]==undefined)
    {
      console.log("creating sel_line");
      this.sel_line_create();
    }

    /* Initialise the hours clock face */
    if(clockPicker.#interface[clockPicker.SEL_HOURS].disp==undefined)
    {
      console.log("creating hour picker");
      this.create_hourPicker();
    }

    /* Initialise the minutes clock face */
    if(clockPicker.#interface[clockPicker.SEL_MINS].disp==undefined)
    {
      console.log("creating mins picker");
      this.create_minsPicker();
    }

    /* Open the default window */
    this.hourSelect();

    /* Let the end user know what's happening */
    console.log("initialised...");
  }
  hourSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_HOURS)
    {
      clockPicker.#interface[this.#picker].disp.style.visibility="hidden";
    }
    this.#picker=clockPicker.SEL_HOURS;
    this.#curInt=clockPicker.#interface[clockPicker.SEL_HOURS];
    clockPicker.#interface[clockPicker.SEL_HOURS].disp.style.visibility="visible";
    this.setCurItem(this);
  }
  minSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_MINS)
    {
      clockPicker.#interface[this.#picker].disp.style.visibility="hidden";
    }
    this.#picker=clockPicker.SEL_MINS;
    this.#curInt=clockPicker.#interface[clockPicker.SEL_MINS];
    clockPicker.#interface[clockPicker.SEL_MINS].disp.style.visibility="visible";
    this.setCurItem(this);
  }
  /**
   * @param {HTMLDivElement} pElement
   */
  setParent = function(pElement)
  {
    if(pElement!=undefined)
    {
      pElement.appendChild(clockPicker.#parentdiv);
    }
  }
  isVisible=function()
  {
    return clockPicker.#parentdiv.style.visibility === "visible";
  }
  hide=function()
  {
    clockPicker.#parentdiv.style.visibility="hidden";
    clockPicker.#hands[0].style.visibility="hidden";
    clockPicker.#hands[1].style.visibility="hidden";
    clockPicker.#interface[0].disp.style.visibility="hidden";
    clockPicker.#interface[1].disp.style.visibility="hidden";
  }
  show=function()
  {
    clockPicker.#parentdiv.style.visibility="visible";
    this.hourSelect();
  }
  /* =========================================================== */
  setCurItem(_this, item)
  {
    [...document.getElementsByClassName('sel')].forEach(el =>
    {
      el.classList.remove("sel");
    });

    let num=-1;
    if(item!=undefined)
    {
      if(typeof item==="number")
      {
        num=item;
      }
      else if(typeof item==="object"&&typeof item.dataset.value)
      {
        num=item.dataset.value;
      }
    }

    if((num>=0)&&num<_this.#curInt.items.length)
    {
      _this.#curInt.sel_tgt=_this.#curInt.items[num];
      let line=document.getElementById('sel_line');
      if(line!=undefined)
      {
        line.style.visibility='visible';
      }
      (_this.#curInt.items[num]).classList.add("sel");
      _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[num], document.getElementById('sel_line'));

      if(clockPicker.SEL_HOURS===_this.#picker)
      {
        _this.time.hours=("00"+_this.#curInt.sel_tgt.dataset.value).slice(-2);
      }
      else if(clockPicker.SEL_MINS===_this.#picker)
      {
        _this.time.mins=("00"+_this.#curInt.sel_tgt.dataset.value).slice(-2);
      }
    }
    else
    {
      let line=document.getElementById('sel_line');
      if(line!=undefined)
      {
        line.style.visibility='hidden';
      }
    }
  }
  create_centre_pin()
  {
    clockPicker.#pin=document.createElement("div");
    clockPicker.#pin.classList.add("clock", "pin");
    clockPicker.#pin.id=("pin");
    /* We need to take into account pin dimensions */
    clockPicker.#pin.style.left=`${this.#offsetToParentCenter}px`;
    clockPicker.#pin.style.top=`${this.#offsetToParentCenter}px`;
    clockPicker.#parentdiv.appendChild(clockPicker.#pin);
  }
  /* =========================================================== */
  foc_line_create()
  {
    clockPicker.#hands[0]=document.createElement('hr');
    clockPicker.#hands[0].style.visibility='hidden';
    clockPicker.#hands[0].classList.add("clock", "clock_hand", "foc_line");
    clockPicker.#hands[0].id=("foc_line");
    clockPicker.#parentdiv.appendChild(clockPicker.#hands[0]);
  }
  /* =========================================================== */
  sel_line_create()
  {
    clockPicker.#hands[1]=document.createElement('hr');
    clockPicker.#hands[1].classList.add("clock", "clock_hand", "sel_line");
    clockPicker.#hands[1].id=("sel_line");
    clockPicker.#parentdiv.appendChild(clockPicker.#hands[1]);
  }
  /* =========================================================== */
  foc_line_hide()
  {
    [...document.getElementsByClassName('focus')].forEach(el =>
    {
      el.classList.remove("focus");
    });
    let line=document.getElementById('foc_line');
    if(line!=undefined)
    {
      line.style.visibility='hidden';
    }
  }
  /* =========================================================== */
  adjustLine(from, to, line)
  {
    if(!from||!to||!line)
    {
      return;
    }

    let a={x: (from.offsetTop+(from.offsetHeight/2))-1, y: (from.offsetLeft+(from.offsetWidth/2))-2};
    let b={x: (to.offsetTop+(to.offsetHeight/2))-1, y: (to.offsetLeft+(to.offsetWidth/2))-2};

    let CA=Math.abs(b.x-a.x);
    let CO=Math.abs(b.y-a.y);
    let H=Math.sqrt(CA*CA+CO*CO);
    let ANG=180/Math.PI*Math.acos(CA/H);

    let br=to.getBoundingClientRect();
    const x=br.x-this.#centre.x;
    const y=br.y-this.#centre.y;
    const nx=x*Math.cos(-90*Math.PI/180)-y*Math.sin(-90*Math.PI/180);
    const ny=x*Math.sin(-90*Math.PI/180)+y*Math.cos(-90*Math.PI/180);
    const angle=Math.atan2(ny, nx)+Math.PI;

    if(b.x>a.x)
    {
      var top=(b.x-a.x)/2+a.x;
    }
    else
    {
      var top=(a.x-b.x)/2+b.x;
    }

    if(b.y>a.y)
    {
      var left=(b.y-a.y)/2+a.y;
    }
    else
    {
      var left=(a.y-b.y)/2+b.y;
    }

    if((a.x<b.x&&a.y<b.y)||(b.x<a.x&&b.y<a.y)||(a.x>b.x&&a.y>b.y)||(b.x>a.x&&b.y>a.y))
    {
      ANG*=-1;
    }

    top-=H/2;

    line.style["-webkit-transform"]='rotate('+ANG+'deg)';
    line.style["-moz-transform"]='rotate('+ANG+'deg)';
    line.style["-ms-transform"]='rotate('+ANG+'deg)';
    line.style["-o-transform"]='rotate('+ANG+'deg)';
    line.style["-transform"]='rotate('+ANG+'deg)';
    line.style.top=top+'px';
    line.style.left=left+'px';
    line.style.height=H+'px';
  }
  /* =========================================================== */
  getDistance(x1, y1, x2, y2)
  {
    let y=x2-x1;
    let x=y2-y1;
    return Math.sqrt(x*x+y*y);
  }
  /* =========================================================== */
  roundNumber(number, digits)
  {
    var multiple=Math.pow(10, digits);
    var rndedNum=Math.round(number*multiple)/multiple;
    return rndedNum;
  }
  get_seg_xy(div, i, r)
  {
    return {
      x: Math.cos((div*i+-90)*(Math.PI/180))*r,
      y: Math.sin((div*i+-90)*(Math.PI/180))*r
    };
  }
  /* =========================================================== */
  create_minsPicker()
  {
    let div=360/60;
    clockPicker.#interface[clockPicker.SEL_MINS].disp=document.createElement("div");
    let wrapper=clockPicker.#interface[clockPicker.SEL_MINS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    wrapper.style.visibility="visible";
    clockPicker.#parentdiv.append(wrapper);

    clockPicker.#interface[clockPicker.SEL_MINS].items=new Array();
    for(let i=0; i<=59; i++)
    {
      let z=((i%5)===0);
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.dataset.value=i;
      segment.innerText=i;
      segment.style.position='absolute';
      let seg=this.get_seg_xy(div, i, clockPicker.#outerRadius);
      segment.style.top=(seg.y+this.#numOffset).toString()+"px";
      segment.style.left=(seg.x+this.#numOffset).toString()+"px";
      if(z)
      {
        segment.classList.add("cardinal");
      }
      clockPicker.#interface[clockPicker.SEL_MINS].items[i]=segment;
      wrapper.appendChild(segment);
    }
  }
  /* =========================================================== */
  create_hourPicker()
  {
    /* Draw the outer ring for AM in 24 hour notation */
    let div=360/12;
    clockPicker.#interface[clockPicker.SEL_HOURS].disp=document.createElement("div");
    let wrapper=clockPicker.#interface[clockPicker.SEL_HOURS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    wrapper.style.visibility="hidden";
    clockPicker.#parentdiv.append(wrapper);

    clockPicker.#interface[clockPicker.SEL_HOURS].items=new Array();
    for(let i=1; i<=12; i++)
    {
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.style.position='absolute';
      segment.dataset.value=i;
      segment.innerText=i;
      let seg=this.get_seg_xy(div, i, clockPicker.#outerRadius);
      segment.style.top=(seg.y+this.#numOffset).toString()+"px";
      segment.style.left=(seg.x+this.#numOffset).toString()+"px";
      segment.classList.add("cardinal");
      clockPicker.#interface[clockPicker.SEL_HOURS].items[i]=segment;
      wrapper.appendChild(segment);
    }
    /* Draw the inner ring for PM in 24 hour notation */
    for(let i=13; i<=24; i++)
    {
      let v=i;
      if(v===24)
      {
        v=0;
      }
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.style.position='absolute';
      segment.dataset.value=v;
      segment.innerText=v;
      let seg=this.get_seg_xy(div, i, clockPicker.#innerRadius);
      segment.style.top=(seg.y+this.#numOffset).toString()+"px";
      segment.style.left=(seg.x+this.#numOffset).toString()+"px";
      segment.classList.add("cardinal");
      clockPicker.#interface[clockPicker.SEL_HOURS].items[v]=segment;
      wrapper.appendChild(segment);
    }
  }
  /* =========================================================== */
  onclick(_this, event)
  {
    let el=event.target;

    /* Loose selection (mouse can be clicked anywhere in the clock face)*/
    if(el.classList.contains("clock")&&_this.#curInt.ovr_tgt!=undefined)

    /* Focused selection (mouse must be clicked over desired field) */
    /* Note: Best used with double click to perform loose selection */
    //if (el.classList.contains("seg") && _this.#curInt.ovr_tgt !== undefined)
    {
      _this.setCurItem(_this, _this.#curInt.ovr_tgt);
      _this.foc_line_hide();
    }
  }
  /* =========================================================== */
  ondblclick(_this, event)
  {
    let el=event.target;
    if(el.classList.contains("clock")&&_this.#curInt.ovr_tgt!=undefined)
    {
      _this.setCurItem(_this, _this.#curInt.ovr_tgt);
    }
  }
  /* =========================================================== */
  onmousedown(_this, event)
  {
    if(event.target.classList.contains("seg"))
    {
      _this.#mousedown=true;
      _this.#hasMoved=false;
      _this.#curInt.ini_tgt=event.target;
    }
  }
  /* =========================================================== */
  onmousemove(_this, event)
  {
    /* Stop drag and select */
    window.getSelection().removeAllRanges();

    let el=event.target;

    /* Check and mark movement with the mouse down */
    if(_this.#mousedown===true)
    {
      if(_this.#curInt.ini_tgt!==el)
      {
        _this.#hasMoved=true;
      }
    }

    /* No point processing if we don't have any items in the list */
    if(!_this.#curInt.items.length||_this.#curInt.items.length<1)
    {
      return;
    }
    //console.log(`(${event.offsetX},${event.offsetY})(${event.clientX},${event.clientY})(${event.pageX},${event.pageY})(${event.screenX},${event.screenX})`);

    let hover={x: 0, y: 0};
    if(el.classList.contains("clock-wrapper")===false)
    {
      hover={x: el.offsetLeft+event.offsetX, y: el.offsetTop+event.offsetY};
    }
    else
    {
      hover={x: event.offsetX, y: event.offsetY};
    }

    /* Should never not be true, but lets make sure */
    if(el.classList.contains("clock"))
    {
      // Math, something I suck at
      const x=hover.x-_this.#centre.x;
      const y=hover.y-_this.#centre.y;
      const nx=x*Math.cos(-90*Math.PI/180)-y*Math.sin(-90*Math.PI/180);
      const ny=x*Math.sin(-90*Math.PI/180)+y*Math.cos(-90*Math.PI/180);
      const angle=Math.atan2(ny, nx)+Math.PI;
      let z;
      if(_this.#curInt.items.length===60)
      {
        z=_this.roundNumber(((Math.PI*angle)*3.0), 0);
      }
      else if(_this.#curInt.items.length===24)
      {
        z=_this.roundNumber(((Math.PI*angle)*.6), 0);
        if(z===0)
        {
          z=12;
        }
        if(z<_this.#curInt.items.length)
        {
          let xPos=hover.x-_this.#parentRect.left;
          let yPos=hover.y-_this.#parentRect.top;
          let dist=_this.getDistance(xPos, yPos, _this.#offsetToParentCenter, _this.#offsetToParentCenter);
          if(dist<65)
          {
            z+=12;
          }
          if(z===24)
          {
            z=0;
          }
        }
      }

      /* Sanity check */
      if(isNaN(z)===true||z>_this.#curInt.items.length)
      {
        return;
      }

      _this.#curInt.ovr_tgt=_this.#curInt.items[z];
      if(_this.#pre_tgt!==_this.#curInt.ovr_tgt)
      {
        _this.foc_line_hide();

        /* Only show the "focus"/"hover" line when not over the selected item */
        if(_this.#curInt.sel_tgt!==_this.#curInt.ovr_tgt&&!_this.#mousedown)
        {
          (_this.#curInt.items[z]).classList.add("focus");
          _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[z], document.getElementById('foc_line'));
          let tmpEl=document.getElementById('foc_line');
          if(tmpEl!=undefined)
          {
            tmpEl.style.visibility='visible';
          }
        }

        if((_this.#mousedown)&&el.classList.contains("clock"))
        {
          [...document.getElementsByClassName('sel')].forEach(el =>
          {
            el.classList.remove("sel");
          });
          (_this.#curInt.items[z]).classList.add('sel');
          _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[z], document.getElementById('sel_line'));
          let tmpEl=document.getElementById('sel_line');
          if(tmpEl!=undefined)
          {
            tmpEl.style.visibility='visible';
          }
        }
        _this.#pre_tgt=_this.#curInt.ovr_tgt;
      }
    }
  }
  onmouseleave(_this, event)
  {
    if(_this.#mousedown===true)
    {
      _this.setCurItem(_this, _this.#curInt.ini_tgt);
      _this.#mousedown=false;
      _this.#hasMoved=false;
    }
    _this.foc_line_hide();
    _this.#pre_tgt=undefined; /* clear this for when we re-enter */
  }
  /* =========================================================== */
  handleEvent(event)
  {
    /* Prevent default actions, regardless of what we do next */
    event.preventDefault();

    /* Check if we have a function for this event */
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      obj(this, event);
    }
    /* Else, we run a default command */
    else
    {
      this.#mousedown=false;
    }
  }
}


/* Initialise with parent div and options (work in progress) */

//let x=new clockPicker(document.getElementById("pdiv"));

