monlst=[];
mongrp=[];

/* =========================================================== */
function init_monthSel()
{
  // Remove any existing class associations
  // ------------------------------------------------
  let g=mongrp.length;
  if(g>0)
  {
    for(let i=0; i<g; i++)
    {
      let oldNode=document.getElementById(mongrp[i].id);
      let newNode=oldNode.cloneNode(true);
      oldNode.parentNode.insertBefore(newNode, oldNode);
      oldNode.parentNode.removeChild(oldNode);
      mongrp[i]=undefined;
    }
    mongrp=[];
  }

  monlst=document.getElementsByClassName("monthsel");
  let l=monlst.length;
  for(let i=0; i<l; i++)
  {
    monlst[i]=new monthSel(monlst[i]);
  }
}

function append_monthSel(el)
{
  let l=mongrp.length;
  for(let i=0; i<l; i++)
  {
    if(mongrp[i].id===el.id)
    {
      return;
    }
  }
  mongrp[l]=new monthSel(el);
}

var monthMenu=undefined;
class monthSel
{
  static #initialised=false;
  #months={
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
  };

  constructor(ctarget)
  {
    this.target=ctarget;
    this.value='';
    this.header=undefined;
    let mon=0;
    try
    {
      mon=ctarget.getAttribute('data-value');
    }
    catch
    {
      console.error('could not read data-value');
    }
    ctarget.classList.add('dropdown-container');
    /*** *** Create toggle switch *** ***/
    this.header=document.createElement('div');
    //this.header.classList.add('dropdown-toggle', 'hover-dropdown');
    this.header.classList.add('dropdown-toggle', 'click-dropdown');
    this.header.innerHTML=this.#months.menu[mon].name;
    ctarget.appendChild(this.header);
    /*** *** Create dropdown menu *** ***/
    let d=document.createElement('div');
    d.classList.add('dropdown-menu');
    ctarget.appendChild(d);
    if(monthMenu===undefined)
    {
      console.log('creating menu');
      monthMenu=document.createElement('ul');
      //monthMenu.classList.add('menuList');
      this.parseMenu(monthMenu, this.#months.menu);
    }
    // Prevent duplicating functions for each element we add
    if(monthSel.#initialised===false)
    {
      monthSel.#initialised=true;
      monthMenu.addEventListener("mouseleave", this);
    }
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
      if(monthMenu.parentNode!==m)
      {
        m.appendChild(monthMenu);
      }
      /**** **** **** ****/
      if(event.target.nextElementSibling.classList.contains('dropdown-active')===true)
      {
        event.target.parentElement.classList.remove('dropdown-open');
        event.target.nextElementSibling.classList.remove('dropdown-active');
      }
      else
      {
        _this.closeDropdown();
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
        _this.header.innerHTML=_this.#months.menu[tgt.value].name;
      }
      _this.closeDropdown();
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
      _this.closeDropdown();
      event.target.parentElement.classList.add('dropdown-open');
      event.target.nextElementSibling.classList.add('dropdown-active');
    }
  }
  onmouseleave(_this, event)
  {
    _this.closeDropdown();
  }
  closeDropdown()
  {
    // remove the open and active class from other opened Dropdown (Closing the opend DropDown)
    document.querySelectorAll('.dropdown-container').forEach(function(container)
    {
      container.classList.remove('dropdown-open');
    });

    document.querySelectorAll('.dropdown-menu').forEach(function(menu)
    {
      menu.classList.remove('dropdown-active');
    });
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
