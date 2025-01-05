/* jshint esversion: 8 */

var shrdGrp=[];

/* =========================================================== */
function init_sharedSel()
{
  // Remove any existing class associations
  // ------------------------------------------------
  let g=shrdGrp.length;
  if(g>0)
  {
    for(let i=0; i<g; i++)
    {
      let oldNode=document.getElementById(shrdGrp[i].id);
      let newNode=oldNode.cloneNode(true);
      oldNode.parentNode.insertBefore(newNode, oldNode);
      oldNode.parentNode.removeChild(oldNode);
      shrdGrp[i]=undefined;
    }
    mongrp=[];
  }

  let lst=document.getElementsByClassName("sharedSel");
  let l=lst.length;
  for(let i=0; i<l; i++)
  {
    lst[i]=new sharedSel(lst[i]);
  }
}
function append_sharedSel(el)
{
  let l=shrdGrp.length;
  for(let i=0; i<l; i++)
  {
    if(shrdGrp[i].id===el.id)
    {
      return;
    }
  }
  shrdGrp[l]=new sharedSel(el);
}
// https://stackoverflow.com/questions/175739/how-can-i-check-if-a-string-is-a-valid-number
function isNumeric(str)
{
  if(typeof str!="string") return false; // we only process strings!
  return !isNaN(str)&& // use type coercion to parse the _entirety_ of the string (`parseFloat` alone does not do this)...
    !isNaN(parseFloat(str)); // ...and ensure strings of whitespace fail
}
class sharedSel
{
  static #initialised=false;
  static #_menus=[];
  static options=[
    {
      "name": "month",
      "menu": [
        {"name": "January", "value": 0, "submenu": null},
        {"name": "February", "value": 1, "submenu": null},
        {"name": "March", "value": 2, "submenu": null},
        {"name": "April", "value": 3, "submenu": null},
        {"name": "May", "value": 4, "submenu": null},
        {"name": "June", "value": 5, "submenu": null},
        {"name": "July", "value": 6, "submenu": null},
        {"name": "August", "value": 7, "submenu": null},
        {"name": "September", "value": 8, "submenu": null},
        {"name": "October", "value": 9, "submenu": null},
        {"name": "November", "value": 10, "submenu": null},
        {"name": "December", "value": 11, "submenu": null}
      ]
    },
    {
      "name": "day",
      "menu": [
        {"name": "Monday", "value": 0, "submenu": null},
        {"name": "Tuesday", "value": 1, "submenu": null},
        {"name": "Wednesday", "value": 2, "submenu": null},
        {"name": "Thursday", "value": 3, "submenu": null},
        {"name": "Friday", "value": 4, "submenu": null},
        {"name": "Saturday", "value": 5, "submenu": null},
        {"name": "Sunday", "value": 6, "submenu": null}
      ]
    },
    {
      "name": "date",
      "menu": [
        {"name": "1st", "value": 0, "submenu": null},
        {"name": "2nd", "value": 1, "submenu": null},
        {"name": "3rd", "value": 2, "submenu": null},
        {"name": "4th", "value": 3, "submenu": null},
        {"name": "5th", "value": 4, "submenu": null},
        {"name": "6th", "value": 5, "submenu": null},
        {"name": "7th", "value": 6, "submenu": null},
        {"name": "8th", "value": 7, "submenu": null},
        {"name": "9th", "value": 8, "submenu": null},
        {"name": "10th", "value": 9, "submenu": null},
        {"name": "11th", "value": 10, "submenu": null},
        {"name": "12th", "value": 11, "submenu": null},
        {"name": "13th", "value": 12, "submenu": null},
        {"name": "14th", "value": 13, "submenu": null},
        {"name": "15th", "value": 14, "submenu": null},
        {"name": "16th", "value": 15, "submenu": null},
        {"name": "17th", "value": 16, "submenu": null},
        {"name": "18th", "value": 17, "submenu": null},
        {"name": "19th", "value": 18, "submenu": null},
        {"name": "20th", "value": 19, "submenu": null},
        {"name": "21st", "value": 20, "submenu": null},
        {"name": "22nd", "value": 21, "submenu": null},
        {"name": "23rd", "value": 22, "submenu": null},
        {"name": "24th", "value": 23, "submenu": null},
        {"name": "25th", "value": 24, "submenu": null},
        {"name": "26th", "value": 25, "submenu": null},
        {"name": "27th", "value": 26, "submenu": null},
        {"name": "28th", "value": 27, "submenu": null},
        {"name": "29th", "value": 28, "submenu": null, "classes": ["noleap"]},
        {"name": "30th", "value": 29, "submenu": null, "classes": ["feb"]},
        {"name": "31st", "value": 30, "submenu": null, "classes": ["feb", "ajsn"]},
      ]
    }];
  constructor(ctarget)
  {
    this.header='';
    this.target=ctarget;
    this.menu=-1;
    this.val=-1;

    // We need to ensure this is done first
    if(sharedSel.#initialised===false)
    {
      sharedSel.#initialised=true;
      console.log('creating menus');
      for(let i=0; i<sharedSel.options.length; i++)
      {
        sharedSel.#_menus[i]=document.createElement('ul');
        console.log(`parsing: ${sharedSel.options[i].name}`);
        this.parseMenu(sharedSel.#_menus[i], sharedSel.options[i].menu);
        sharedSel.#_menus[i].addEventListener("mouseleave", this);
      }
    }
    var x='';
    try
    {
      x=ctarget.getAttribute('data-value');
      x=x.split(':');
    }
    catch
    {
      console.error('Error parsing data-value');
      return false;
    }

    // Check if the user knows which index they want
    if(isNumeric(x[0]))
    {
      this.menu=Number(x[0]);
      if(this.menu<0||this.menu>=sharedSel.options.length)
      {
        this.menu=-2;
      }
    }
    // Ekse we are looking for our target by name
    else
    {
      for(var i=0; i<sharedSel.options.length&&this.menu<0; i++)
      {
        if(x[0]===sharedSel.options[i].name)
        {
          this.menu=i;
        }
      }
    }

    // Second field should always be a number
    if(isNumeric(x[1]))
    {
      this.val=Number(x[1]);
    }

    // Check both required values were set
    if(this.menu<0||this.val<0)
    {
      //console.error(`Failed to parse menu item: ${this.menu}:${this.val}`);
      console.error(`Failed to parse menu item: ${ctarget}`);
      return;
    }
    // Sanitize input
    else if(this.val<0||this.val>(sharedSel.options[this.menu].menu.length-1))
    {
      this.val=0;
    }

    ctarget.classList.add('dropdown-container');
    /*** *** Create toggle switch *** ***/
    this.header=document.createElement('div');
    //this.header.classList.add('dropdown-toggle', 'hover-dropdown');
    this.header.classList.add('dropdown-toggle', 'click-dropdown');
    this.header.innerHTML=sharedSel.options[this.menu].menu[this.val].name;
    ctarget.appendChild(this.header);
    /*** *** Create dropdown menu *** ***/
    let d=document.createElement('div');
    d.classList.add('dropdown-menu');
    ctarget.appendChild(d);

    ctarget.addEventListener("click", this);
    // For opening on hover
    //ctarget.addEventListener("mouseover", this);
    //this.header.addEventListener("mouseleave", this);
  }
  /**** **** Menu Control **** ****/
  parseMenu(element, menu)
  {
    for(var i=0; i<menu.length; i++)
    {
      var nestedli=document.createElement('li');
      nestedli.classList.add('dropdown-item');
      if(typeof menu[i].classes!==undefined)
      {
        if(menu[i].classes instanceof Array)
        {
          for(const _el of menu[i].classes)
          {
            nestedli.classList.add(_el);
          }
        }
      }
      nestedli.value=menu[i].value;
      nestedli.appendChild(document.createTextNode(menu[i].name));

      if(menu[i].submenu!=null)
      {
        var subul=document.createElement('ul');
        nestedli.appendChild(subul);
        parseMenu(subul, menu[i].submenu);
      }
      element.appendChild(nestedli);
    }
  }
  /**** **** Shared control **** ****/
  onclick(_this, event)
  {
    let tgt=event.target.parentNode;
    if(event.target.classList.contains('click-dropdown')===true)
    {
      let m=event.target.parentNode.querySelectorAll('.dropdown-menu')[0];
      if(sharedSel.#_menus[_this.menu].parentNode!==m)
      {
        m.appendChild(sharedSel.#_menus[_this.menu]);
      }
      /**** **** **** ****/
      if(event.target.nextElementSibling.classList.contains('dropdown-active')===true)
      {
        event.target.parentElement.classList.remove('dropdown-open');
        event.target.nextElementSibling.classList.remove('dropdown-active');
      }
      else
      {
        closeDropdown();
        event.target.parentElement.classList.add('dropdown-open');
        event.target.nextElementSibling.classList.add('dropdown-active');
      }
    }
    else
    {
      if(event.target.classList.contains('dropdown-item'))
      {
        let tgt=event.target;
        _this.value=tgt.value;
        _this.header.innerText=tgt.innerText;
      }
      closeDropdown();
    }
    return false;
  }
  onmouseover(_this, event)
  {
    let tgt=event.target.parentNode;
    if(event.target.classList.contains('hover-dropdown')===true)
    {
      let m=event.target.parentNode.querySelectorAll('.dropdown-menu')[0];
      if(monthMenu.parentNode!==m)
      {
        m.appendChild(monthMenu);
      }
      closeDropdown();
      event.target.parentElement.classList.add('dropdown-open');
      event.target.nextElementSibling.classList.add('dropdown-active');
    }
  }
  onmouseleave(_this, event)
  {
    closeDropdown();
  }
  onwheel(_this, event)
  {
    const delta=Math.sign(event.deltaY);
    let el=event.target;
    if(el.classList.contains("time"))
    {
      if(el.classList.contains("monthsel"))
      {
        el.value=String(Number(el.value)-delta);
      }
    }
  }
  handleEvent(event)
  {
    event.preventDefault();
    let obj=this["on"+event.type];
    if(typeof obj==="function")
    {
      obj(this, event);
    }
  }
}
