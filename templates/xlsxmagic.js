const TIME_OUT_FOR_EACH_CHUNKS = 2000
const DELAY_TIME_FOR_EACH_LOOP = 500
var indexCurrentSheet = 0
var tempSetTimeOut = null
var currentSheetEle = null
var currentTableChunkEle = null
var currentTheadChunkEle = null
var currentTbodyChunkEle = null
google.charts.load('current', {'packages':['corechart']})
function readTextFile(file, file_type, callback, callbackfail) {
  var rawFile = new XMLHttpRequest()
  if (file_type === "json") {
    rawFile.overrideMimeType("application/json")
  } else {
    rawFile.overrideMimeType("text/plain")
  }
  rawFile.open("GET", file, true)
  rawFile.onreadystatechange = function() {
    if (rawFile.readyState === 4) {
      callback(rawFile.responseText);
    }
    else{
      callbackfail()
    }
  }
  rawFile.onerror = function () {
    callbackfail()
  }
  rawFile.send(null)
}
function loadMergedCells(indexCurrentSheet, startTime) {
  var startTime = startTime
  var isFailed = false
  var isDone = false
  var mergedCellsFileName = "chunk_" + indexCurrentSheet + "_mc"
  console.log(mergedCellsFileName)
  var currentDivChunkEle = document.getElementById(mergedCellsFileName)
  if (currentDivChunkEle === null) {
    return
  }
  var URL_JSON_CHUNK = currentDivChunkEle.getAttribute('data-chunk-url')
  var _data = null
  readTextFile(URL_JSON_CHUNK, 'json', function(data){
    if(typeof data != 'undefinded') {
      startTime = new Date().getTime()
      _data = JSON.parse(data)
      isDone = true
    } else {
      var endTime = new Date().getTime()
      if (endTime - startTime  >= TIME_OUT_FOR_EACH_CHUNKS) {
        isFailed = true
      }
      else {
        tempSetTimeOut = setTimeout(function(){loadMergedCells(indexCurrentSheet, startTime)}, DELAY_TIME_FOR_EACH_LOOP)
      }
    }
  }, function(){
       // handle fail
       var endTime = new Date().getTime()
       if (endTime - startTime  >= TIME_OUT_FOR_EACH_CHUNKS) {
         isFailed = true
       }
       else{
         tempSetTimeOut = setTimeout(function(){loadMergedCells(indexCurrentSheet, startTime)}, DELAY_TIME_FOR_EACH_LOOP)
       }
     })
  if (isDone) {
    return _data
  }
  if (isFailed) {
    return
  }
}
function loadChunks(indexCurrentSheet, indexCurrentChunk, startTime) {
  var indexCurrentChunk = indexCurrentChunk;
  var startTime = startTime
  var isFailed = false
  var isDone = false
  var htmlFileName = "chunk_" + indexCurrentSheet + "_" + indexCurrentChunk
  var currentDivChunkEle = document.getElementById(htmlFileName)
  if (currentDivChunkEle === null) {
    return
  }
  var URL_HTML_CHUNK = currentDivChunkEle.getAttribute('data-chunk-url')
  readTextFile(URL_HTML_CHUNK, 'html', function(data){
    if(typeof data != 'undefinded') {
      startTime = new Date().getTime()
      if (indexCurrentChunk === 0) {
	currentTheadChunkEle.innerHTML = data
      } else if (indexCurrentChunk == 1) {
	currentTbodyChunkEle.innerHTML = data
      }
      indexCurrentChunk++
      loadChunks(indexCurrentSheet, indexCurrentChunk, startTime)
    } else {
      var endTime = new Date().getTime()
      if (endTime - startTime  >= TIME_OUT_FOR_EACH_CHUNKS) {
        isFailed = true
      }
      else {
        tempSetTimeOut = setTimeout(function(){loadChunks(indexCurrentSheet, indexCurrentChunk, startTime)}, DELAY_TIME_FOR_EACH_LOOP)
      }
    }
  }, function(){
       // handle fail
       var endTime = new Date().getTime()
       if (endTime - startTime  >= TIME_OUT_FOR_EACH_CHUNKS) {
         isFailed = true
       }
       else{
         tempSetTimeOut = setTimeout(function(){loadChunks(indexCurrentSheet, indexCurrentChunk, startTime)}, DELAY_TIME_FOR_EACH_LOOP)
       }
     })
  if (isFailed || isDone) {
    return;
  }
}
function applyMergedCells(mergedCellsData) {
  for (var key in mergedCellsData) {
    var mergedCellEle = document.getElementById(indexCurrentSheet + '_' + key)
    mergedCellEle.colSpan = mergedCellsData[key].colspan
    mergedCellEle.rowSpan = mergedCellsData[key].rowspan
    var countColSpan = mergedCellsData[key].colspan
    var rowEle = mergedCellEle.parentNode
    while (countColSpan > 1) {
      var removedMergeCellEle = mergedCellEle.nextElementSibling
      rowEle.removeChild(removedMergeCellEle)
      countColSpan--
    }
    var indexMergedCellEle = Array.prototype.indexOf.call(rowEle.children, mergedCellEle)
    var countRowSpan = mergedCellsData[key].rowspan
    while (countRowSpan > 1) {
      var countColSpan = mergedCellsData[key].colspan
      rowEle = rowEle.nextElementSibling
      rowEle.children[indexMergedCellEle].colspan = mergedCellsData[key].colspan
      while (countColSpan > 1) {
        var removedMergeCellEle = rowEle.children[indexMergedCellEle + 1]
        rowEle.removeChild(removedMergeCellEle)
	countColSpan--
      }
      countRowSpan--
    }
  }
}
function Viewer() {
  currentSheetEle = document.getElementById('sheet_' + indexCurrentSheet)
  currentSheetEle.style.removeProperty("display")
  if (!document.getElementById('tb_' + indexCurrentSheet)) {
    currentTableChunkEle = currentSheetEle.appendChild(document.createElement('table'))
    currentTableChunkEle.id = "tb_" + indexCurrentSheet
    currentTheadChunkEle = currentTableChunkEle.appendChild(document.createElement('thead'))
    currentTbodyChunkEle = currentTableChunkEle.appendChild(document.createElement('tbody'))
    var mergedCellsData = loadMergedCells(indexCurrentSheet, new Date().getTime())
    loadChunks(indexCurrentSheet, 0, new Date().getTime())
    if (mergedCellsData) {
      applyMergedCells(mergedCellsData)
    }
  }
}
function handleButtonClick(event) {
  var btnClicked = document.getElementById(event.srcElement.id)
  if ('btn_' + indexCurrentSheet == btnClicked.id) {
    return
  }
  currentSheetEle.style.display = 'none'
  var preBtn = document.getElementById('btn_' + indexCurrentSheet)
  preBtn.style.fontWeight = 'normal'
  indexCurrentSheet = btnClicked.id.split("btn_")[1]
  btnClicked.style.fontWeight = 'bold'
  Viewer()
}
document.addEventListener('DOMContentLoaded', function() {
  Viewer()
}, false);
