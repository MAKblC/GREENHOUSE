/* пример таблицы
https://docs.google.com/spreadsheets/d/1GDqGhaKMM04tEa1RAZmXqHPdEMKx_wwgVC39gFtoE3E/edit?usp=sharing
*/

// СКРИПТ

// запостить данные с датчиков
function doPost(e) {
var ss = SpreadsheetApp.openById("1GXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX3E");
var sheet = ss.getSheetByName("ESP_sensor");
var date = String(e.parameter.date);
var light = Number(e.parameter.light);
var temp = Number(e.parameter.temp);
var hum = Number(e.parameter.hum);
var press = Number(e.parameter.press);
var tempH = Number(e.parameter.tempH);
var humH = Number(e.parameter.humH);
var uvi = Number(e.parameter.uvi);
sheet.appendRow([date,light,temp,hum,press,tempH,humH,uvi]);
}

// считать параметры для управления
function doGet(e){
var read = e.parameter.read;
if (read !== undefined){
// считать 9 значений в ячейках от A1(1,1) до С3(3,3)
return ContentService.createTextOutput(SpreadsheetApp.openById('1GXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX3E').getSheetByName('ESP_sensor').getRange(4, 2, 1, 6).getValues());
// считать ячейку C2
// return ContentService.createTextOutput(sheet.getRange('C2').getValues());
}
}

// сменить цвет итоговой RGB-ячейки
function onEdit(e){
  var column_number = e.source.getActiveSheet().getActiveRange().getColumn();
  //check if one of the first three columns (R, G, B) is edited
  if(column_number == 5 || column_number == 6 || column_number == 7){

    var s = SpreadsheetApp.getActiveSheet();
    var row_number = e.source.getActiveSheet().getActiveRange().getRowIndex();

    //Check if all the three values are filled (r,g,b)
    if( !s.getRange(row_number,5).isBlank() && !s.getRange(row_number,6).isBlank() && !s.getRange(row_number,7).isBlank()){
      
      var r = parseInt(s.getRange(row_number,5).getValue());
      var g = parseInt(s.getRange(row_number,6).getValue());
      var b = parseInt(s.getRange(row_number,7).getValue());

      s.getRange(row_number,8).setBackgroundRGB(r,g,b);
     // s.getRange(row_number,8).setValue(s.getRange(row_number,8).getBackground());
    }
  }
}

