// https://www.w3schools.com/howto/howto_js_draggable.asp
// https://www.w3schools.com/howto/tryit.asp?filename=tryhow_js_draggable
function dragElement(elmnt)
{
  var pos1=0, pos2=0, pos3=0, pos4=0;
  if(document.getElementById(elmnt.id+"header"))
  {
    /* if present, the header is where you move the DIV from:*/
    document.getElementById(elmnt.id+"header").onmousedown=dragMouseDown;
  } else
  {
    /* otherwise, move the DIV from anywhere inside the DIV:*/
    elmnt.onmousedown=dragMouseDown;
  }

  function dragMouseDown(e)
  {
    e=e||window.event;
    e.preventDefault();
    // get the mouse cursor position at startup:
    pos3=e.clientX;
    pos4=e.clientY;
    document.onmouseup=closeDragElement;
    // call a function whenever the cursor moves:
    document.onmousemove=elementDrag;
  }

  function elementDrag(e)
  {
    e=e||window.event;
    e.preventDefault();
    // calculate the new cursor position:
    pos1=pos3-e.clientX;
    pos2=pos4-e.clientY;
    pos3=e.clientX;
    pos4=e.clientY;
    // set the element's new position:
    elmnt.style.top=(elmnt.offsetTop-pos2)+"px";
    elmnt.style.left=(elmnt.offsetLeft-pos1)+"px";
  }

  function closeDragElement()
  {
    /* stop moving when mouse button is released:*/
    document.onmouseup=null;
    document.onmousemove=null;
  }
}

/*
function Dragger(element)
{
  this.element=element;
  this.x=0;
  this.y=0;
  this.element.addEventListener('mousedown', this);
}

// trigger .ontype from event.type, like onmousedown
Dragger.prototype.handleEvent=function(event)
{
  var method='on'+event.type;
  // call method if there
  if(this[method])
  {
    this[method](event);
  }
};

Dragger.prototype.onmousedown=function(event)
{
  this.dragStartX=this.x;
  this.dragStartY=this.y;
  this.pointerDownX=event.pageX;
  this.pointerDownY=event.pageY;

  window.addEventListener('mousemove', this);
  window.addEventListener('mouseup', this);
};

Dragger.prototype.onmousemove=function(event)
{
  var moveX=event.pageX-this.pointerDownX;
  var moveY=event.pageY-this.pointerDownY;
  this.x=this.dragStartX+moveX;
  this.y=this.dragStartY+moveY;
  this.element.style.left=this.x+'px';
  this.element.style.top=this.y+'px';
};

Dragger.prototype.onmouseup=function()
{
  window.removeEventListener('mousemove', this);
  window.removeEventListener('mouseup', this);
};

// --------------- //

var dragElems=document.querySelectorAll('.draggable');
for(var i=0; i<dragElems.length; i++)
{
  var dragElem=dragElems[i];
  new Dragger(dragElem);
}
*/