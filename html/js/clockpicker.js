


function closest(to, selector)
{
  //let currentElement=document.querySelector(to);
  let currentElement=to;
  let returnElement;

  while(currentElement.parentNode&&!returnElement)
  {
    currentElement=currentElement.parentNode;
    returnElement=currentElement.querySelector(selector);
  }

  return returnElement;
}
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
  /* Constants */
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

  #parentdiv;
  #numOffset;
  #parentRect;
  #offsetToParentCenter;
  #centre;

  #mousedown;
  #hasMoved;
  /* Previous target we had the mouse over */
  #pre_tgt;

  #interface;
  #picker;
  #curInt;

  /* =========================================================== */
  constructor(tElement, options=null)
  {
    /* Object manipulation */
    this.#mousedown=false;
    this.#hasMoved=false;
    this.#pre_tgt=null;

    /* Display coordination */
    this.#picker=clockPicker.SEL_HOURS;
    /**
     * @property {array|null} items
     */
    this.#interface=[
      {items: null, disp: null, ovr_tgt: null, sel_tgt: null, ini_tgt: null},
      {items: null, disp: null, ovr_tgt: null, sel_tgt: null, ini_tgt: null}];
    /* default */
    this.#curInt=this.#interface[this.#picker];

    /* Exposed to the end user */
    this.time={hours: "00", mins: "00"};

    /* should iterate through to see if they are a child of passed element */
    /* (bare with me, this is all work in progress) */
    this.#parentdiv=document.getElementsByClassName("clock-wrapper")[0];
    if(this.#parentdiv!==undefined)
    {
      this.#parentdiv=tElement;
    }
    else
    {
      /* Doing it this way for now, so I can work on bounding rectangle */
      this.#parentdiv=document.createElement("div");
      this.#parentdiv.classList.add("clock", "clock-wrapper");
      this.#parentdiv.style.width=clockPicker.#width;
      this.#parentdiv.style.height=clockPicker.#height;
    }

    /* Configure some basics for later utility */
    this.#parentRect=this.#parentdiv.getBoundingClientRect();
    if(this.#parentRect.width>0)
    {
      this.#offsetToParentCenter=Math.round(this.#parentdiv.offsetWidth/2);
      this.#centre={x: this.#parentRect.left+((this.#parentRect.width-0)/2), y: this.#parentRect.top+((this.#parentRect.height-0)/2)};
      console.log(this.#centre);
    }
    else
    {
      this.#offsetToParentCenter=Math.round(clockPicker.#width/2);
      this.#centre={x: Math.round(clockPicker.#height/2), y: Math.round(clockPicker.#width/2)};
    }
    this.#numOffset=this.#offsetToParentCenter-clockPicker.#offsetToNumCenter;

    console.log(this.#offsetToParentCenter, this.#centre.x, this.#centre.y, this.#numOffset);

    /* Create the centre pin */
    this.create_centre_pin();

    /* Check if we are being added to an element */
    if(tElement!==null)
    {
      tElement.appendChild(this.#parentdiv);
    }

    /* Create the selection "hands" */
    this.foc_line_create();
    this.sel_line_create();

    /* Initialise the display */
    this.create_hourPicker();
    //this.#items = this.#hour;
    this.create_minsPicker();
    //this.#items = this.#mins;

    /* Open the default window */
    this.minSelect();

    /* Add user interaction */
    this.#parentdiv.addEventListener("click", this);
    this.#parentdiv.addEventListener("dblclick", this);
    this.#parentdiv.addEventListener("mousemove", this);
    this.#parentdiv.addEventListener("mousedown", this);
    this.#parentdiv.addEventListener("mouseup", this);
    this.#parentdiv.addEventListener("mouseleave", this);
    console.log("initialised...");
  }
  hourSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_HOURS)
    {
      this.#interface[this.#picker].disp.style.visibility="hidden";
    }
    this.#picker=clockPicker.SEL_HOURS;
    this.#curInt=this.#interface[clockPicker.SEL_HOURS];
    this.#interface[clockPicker.SEL_HOURS].disp.style.visibility="visible";
    this.setCurItem(this);
  };
  minSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_MINS)
    {
      this.#interface[this.#picker].disp.style.visibility="hidden";
    }
    this.#picker=clockPicker.SEL_MINS;
    this.#curInt=this.#interface[clockPicker.SEL_MINS];
    this.#interface[clockPicker.SEL_MINS].disp.style.visibility="visible";
    this.setCurItem(this);
  };
  /* =========================================================== */
  setCurItem(_this, item)
  {
    [...document.getElementsByClassName('sel')].forEach(el =>
    {
      el.classList.remove("sel");
    });

    let num=-1;
    if(item!==null)
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
      if(line!==null)
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
      if(line!==null)
      {
        line.style.visibility='hidden';
      }
    }
  }
  create_centre_pin()
  {
    let childdiv=document.createElement("div");
    childdiv.classList.add("clock", "pin");
    childdiv.id=("pin");
    /* We need to take into account pin dimensions */
    childdiv.style.left=`${this.#offsetToParentCenter}px`;
    childdiv.style.top=`${this.#offsetToParentCenter}px`;
    console.log(childdiv);
    this.#parentdiv.appendChild(childdiv);
  }
  /* =========================================================== */
  foc_line_create()
  {
    var foc_line=document.createElement('hr');
    foc_line.style.visibility='hidden';
    foc_line.classList.add("clock", "clock_hand", "foc_line");
    foc_line.id=("foc_line");
    this.#parentdiv.appendChild(foc_line);
  }
  /* =========================================================== */
  sel_line_create()
  {
    var sel_line=document.createElement('hr');
    sel_line.classList.add("clock", "clock_hand", "sel_line");
    //sel_line.style.zIndex=0;
    sel_line.id=("sel_line");
    this.#parentdiv.appendChild(sel_line);
  }
  /* =========================================================== */
  foc_line_hide()
  {
    [...document.getElementsByClassName('focus')].forEach(el =>
    {
      el.classList.remove("focus");
    });
    let line=document.getElementById('foc_line');
    if(line!==null)
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
    this.#interface[clockPicker.SEL_MINS].disp=document.createElement("div");
    let wrapper=this.#interface[clockPicker.SEL_MINS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    wrapper.style.visibility="visible";
    this.#parentdiv.append(wrapper);

    this.#interface[clockPicker.SEL_MINS].items=new Array();
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
      this.#interface[clockPicker.SEL_MINS].items[i]=segment;
      wrapper.appendChild(segment);
    }
  }
  /* =========================================================== */
  create_hourPicker()
  {
    /* Draw the outer ring for AM in 24 hour notation */
    let div=360/12;
    this.#interface[clockPicker.SEL_HOURS].disp=document.createElement("div");
    let wrapper=this.#interface[clockPicker.SEL_HOURS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    wrapper.style.visibility="hidden";
    this.#parentdiv.append(wrapper);

    this.#interface[clockPicker.SEL_HOURS].items=new Array();
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
      this.#interface[clockPicker.SEL_HOURS].items[i]=segment;
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
      this.#interface[clockPicker.SEL_HOURS].items[v]=segment;
      wrapper.appendChild(segment);
    }
  }
  /* =========================================================== */
  onclick(_this, event)
  {
    let el=event.target;

    /* Loose selection (mouse can be clicked anywhere in the clock face)*/
    if(el.classList.contains("clock")&&_this.#curInt.ovr_tgt!==null)

    /* Focused selection (mouse must be clicked over desired field) */
    /* Note: Best used with double click to perform loose selection */
    //if (el.classList.contains("seg") && _this.#curInt.ovr_tgt !== null)
    {
      _this.setCurItem(_this, _this.#curInt.ovr_tgt);
      _this.foc_line_hide();
    }
  }
  /* =========================================================== */
  ondblclick(_this, event)
  {
    let el=event.target;
    if(el.classList.contains("clock")&&_this.#curInt.ovr_tgt!==null)
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
          document.getElementById('foc_line').style.visibility='visible';
        }

        if((_this.#mousedown)&&el.classList.contains("clock"))
        {
          [...document.getElementsByClassName('sel')].forEach(el =>
          {
            el.classList.remove("sel");
          });
          (_this.#curInt.items[z]).classList.add('sel');
          _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[z], document.getElementById('sel_line'));
          document.getElementById('sel_line').style.visibility='visible';
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

