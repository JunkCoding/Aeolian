

class clockPicker
{
  /* Constants */
  static outerRadius=82;
  static innerRadius=55;
  static offsetToNumCenter=22;
  static SEL_HOURS=0;
  static SEL_MINS=1;

  #parentdiv;
  #numOffset;
  #parentRect;
  #offsetToParentCenter;
  #centre;

  #mousedown;
  #hasMoved;
  #pre_tgt;

  #interface;
  #picker;
  #curInt;

  /* =========================================================== */
  constructor(ctarget)
  {
    /* Object manipulation */
    this.#mousedown=false;
    this.#hasMoved=false;
    this.#pre_tgt=null;

    /* Display coordination */
    this.#parentdiv=ctarget;
    this.#picker=clockPicker.SEL_HOURS;
    this.#interface=[
      {items: null, disp: null, ovr_tgt: 0, cur_sel: 0, cur_tgt: 0},
      {items: null, disp: null, ovr_tgt: 0, cur_sel: 0, cur_tgt: 0}];
    /* default */
    this.#curInt=this.#interface[this.#picker];

    /* Exposed to the end user */
    this.time={hours: 0, mins: 0};

    /* Configure some basics for later utility */
    this.#parentRect=this.#parentdiv.getBoundingClientRect();
    this.#offsetToParentCenter=parseInt(this.#parentdiv.offsetWidth/2); //assumes parent is square
    this.#centre={x: this.#parentRect.left+((this.#parentRect.width-0)/2), y: this.#parentRect.top+((this.#parentRect.height-0)/2)};
    this.#numOffset=this.#offsetToParentCenter-clockPicker.offsetToNumCenter;

    /* Create the centre pin */
    let childdiv=document.createElement("div");
    childdiv.classList.add("clock", "pin");
    childdiv.id=("pin");
    childdiv.style.left=`${this.#offsetToParentCenter}px`;
    childdiv.style.top=`${this.#offsetToParentCenter}px`;
    this.#parentdiv.appendChild(childdiv);

    /* Add user interaction */
    this.#parentdiv.addEventListener("click", this);
    this.#parentdiv.addEventListener("dblclick", this);
    this.#parentdiv.addEventListener("mousemove", this);
    this.#parentdiv.addEventListener("mousedown", this);
    this.#parentdiv.addEventListener("mouseup", this);
    this.#parentdiv.addEventListener("mouseleave", this);
    console.log("initialised...");

    /* Initialise the display */
    this.create_hourPicker();
    //this.#items = this.#hour;
    this.create_minsPicker();
    //this.#items = this.#mins;

    /* Create the selection "hands" */
    this.foc_line_create();
    this.sel_line_create();

    /* Open the default window */
    this.minSelect();
  }
  hourSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_HOURS)
    {
      this.#interface[this.#picker].disp.style.display="none";
    }
    this.#picker=clockPicker.SEL_HOURS;
    this.#curInt=this.#interface[clockPicker.SEL_HOURS];
    this.#interface[clockPicker.SEL_HOURS].disp.style.display="block";
    this.selectCurItem(this);
  };
  minSelect=function()
  {
    if(this.#picker!==clockPicker.SEL_MINS)
    {
      this.#interface[this.#picker].disp.style.display="none";
    }
    this.#picker=clockPicker.SEL_MINS;
    this.#curInt=this.#interface[clockPicker.SEL_MINS];
    this.#interface[clockPicker.SEL_MINS].disp.style.display="block";
    this.selectCurItem(this);
  };
  /* =========================================================== */
  selectCurItem(_this, item=null)
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
      document.getElementById('sel_line').style.visibility='visible';
      (_this.#curInt.items[num]).classList.add('sel');
      _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[num], document.getElementById('sel_line'));
    }
    else
    {
      document.getElementById('sel_line').style.visibility='hidden';
    }
  }
  /* =========================================================== */
  foc_line_create()
  {
    var foc_line=document.createElement('div');
    foc_line.style.visibility='hidden';
    foc_line.classList.add("clock", "foc_line");
    foc_line.style.zIndex=0;
    foc_line.id=("foc_line");
    this.#parentdiv.appendChild(foc_line);
  }
  /* =========================================================== */
  sel_line_create()
  {
    var sel_line=document.createElement('div');
    sel_line.classList.add("clock", "sel_line");
    sel_line.style.zIndex=0;
    sel_line.id=("sel_line");
    this.#parentdiv.appendChild(sel_line);
  }
  /* =========================================================== */
  adjustLine(from, to, line)
  {
    if(!from||!to||!line)
    {
      return;
    }
    let fT=from.offsetTop+from.offsetHeight/2-1;
    let tT=to.offsetTop+to.offsetHeight/2-1;
    let fL=from.offsetLeft+from.offsetWidth/2-2;
    let tL=to.offsetLeft+to.offsetWidth/2-2;
    let CA=Math.abs(tT-fT);
    let CO=Math.abs(tL-fL);
    let H=Math.sqrt(CA*CA+CO*CO);
    let ANG=180/Math.PI*Math.acos(CA/H);
    if(tT>fT)
    {
      var top=(tT-fT)/2+fT;
    }
    else
    {
      var top=(fT-tT)/2+tT;
    }
    if(tL>fL)
    {
      var left=(tL-fL)/2+fL;
    }
    else
    {
      var left=(fL-tL)/2+tL;
    }
    if((fT<tT&&fL<tL)||(tT<fT&&tL<fL)||(fT>tT&&fL>tL)||(tT>fT&&tL>fL))
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
  /* =========================================================== */
  create_minsPicker()
  {
    let div=360/60;
    this.#interface[clockPicker.SEL_MINS].disp=document.createElement("div");
    let wrapper=this.#interface[clockPicker.SEL_MINS].disp;
    wrapper.classList.add("clock", "face-wrapper");
    wrapper.style.display="none";
    this.#parentdiv.append(wrapper);

    this.#interface[clockPicker.SEL_MINS].items=new Array();
    for(let i=0; i<=59; i++)
    {
      let z=((i%5)===0);
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.dataset.value=i;
      segment.innerHTML=i;
      segment.style.position='absolute';
      let x=Math.cos((div*i+-90)*(Math.PI/180))*clockPicker.outerRadius;
      let y=Math.sin((div*i+-90)*(Math.PI/180))*clockPicker.outerRadius;
      segment.style.left=(x+this.#numOffset).toString()+"px";
      segment.style.top=(y+this.#numOffset).toString()+"px";
      if(z)
      {
        segment.style.zIndex=1;
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
    //wrapper.style.display="none";
    this.#parentdiv.append(wrapper);

    this.#interface[clockPicker.SEL_HOURS].items=new Array();
    for(let i=1; i<=12; i++)
    {
      let segment=document.createElement('div');
      segment.classList.add("clock", "seg");
      segment.style.position='absolute';
      segment.dataset.value=i;
      segment.innerHTML=i;
      let x=Math.cos((div*i+-90)*(Math.PI/180))*clockPicker.outerRadius;
      let y=Math.sin((div*i+-90)*(Math.PI/180))*clockPicker.outerRadius;
      segment.style.top=(y+this.#numOffset).toString()+"px";
      segment.style.left=(x+this.#numOffset).toString()+"px";
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
      segment.innerHTML=v;
      let y=Math.sin((div*i+-90)*(Math.PI/180))*clockPicker.innerRadius;
      let x=Math.cos((div*i+-90)*(Math.PI/180))*clockPicker.innerRadius;
      segment.style.top=(y+this.#numOffset).toString()+"px";
      segment.style.left=(x+this.#numOffset).toString()+"px";
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
      _this.selectCurItem(_this, _this.#curInt.ovr_tgt);
    }
  }
  /* =========================================================== */
  ondblclick(_this, event)
  {
    let el=event.target;
    if(el.classList.contains("clock")&&_this.#curInt.ovr_tgt!==null)
    {
      _this.selectCurItem(_this, _this.#curInt.ovr_tgt);
    }
  }
  /* =========================================================== */
  onmousedown(_this, event)
  {
    if(event.target.classList.contains("seg"))
    {
      _this.#mousedown=true;
      _this.#hasMoved=false;
      _this.#curInt.cur_tgt=event.target;
    }
  }
  onmousedown(_this, event)
  {
    _this.#mousedown=true;
  }
  /* =========================================================== */
  onmousemove(_this, event)
  {
    window.getSelection().removeAllRanges();
    let el=event.target;
    if(!_this.#curInt.items.length||_this.#curInt.items.length<1)
    {
      return;
    }
    if(el.classList.contains("clock"))
    {
      // Math, something I suck at
      const x=event.pageX-_this.#centre.x;
      const y=event.pageY-_this.#centre.y;
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
          let xPos=event.clientX-_this.#parentRect.left;
          let yPos=event.clientY-_this.#parentRect.top;
          let dist=_this.getDistance(xPos, yPos, _this.#offsetToParentCenter, _this.#offsetToParentCenter);// _this.#centre.x, _this.#centre.y);
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

      _this.#curInt.ovr_tgt=_this.#curInt.items[z];

      if(_this.#pre_tgt!==_this.#curInt.ovr_tgt)
      {
        [...document.getElementsByClassName('focus')].forEach(el =>
        {
          el.classList.remove("focus");
        });

        (_this.#curInt.items[z]).classList.add('focus');
        _this.adjustLine(document.getElementById('pin'), _this.#curInt.items[z], document.getElementById('foc_line'));
        document.getElementById('foc_line').style.visibility='visible';

        if((_this.#mousedown)&&el.classList.contains("clock"))
        {
          [...document.getElementsByClassName('sel')].forEach(el =>
          {
            el.classList.remove("sel");
          });
          //(_this.#curInt.items[z]).classList.add('sel');
          //_this.adjustLine(document.getElementById('pin'), _this.#curInt.items[z], document.getElementById('sel_line'));
          if(_this.#curInt.cur_sel!=z)
          {
            _this.#hasMoved=true;
          }
          else
          {
            _this.#hasMoved=false;
          }
        }
        _this.#pre_tgt=_this.#curInt.ovr_tgt;
      }
    }
  }
  onmouseleave(_this, event)
  {
    [...document.getElementsByClassName('focus')].forEach(el =>
    {
      el.classList.remove("focus");
    });
    document.getElementById('foc_line').style.visibility='hidden';
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

let x=new clockPicker(document.getElementById("pdiv"));

