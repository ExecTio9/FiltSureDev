
/**
 * Google Apps Script to log environmental sensor data to Google Sheets
 *
 * Project: FiltSure Watch
 * Spreadsheet ID: 1jLbKlstPxHlD7kHZhQn0Z-8PDUdkm7ZQxHZjDr8VWqw
 * Each ESP32 node logs to a dynamic sheet: "UNIT_" + device ID
 *
 * URL Parameters:
 *   sts   - action to perform (write or read)
 *   id    - device ID (used to select/create sheet tab)
 *   bc    - boot count
 *   srs   - status (Success, Failed, etc.)
 *   temp  - temperature in °C
 *   humd  - relative humidity in %
 *   Prs   - pressure in hPa
 *   rfid  - optional RFID tag UID
 *
 *Web app URL                     : https://script.google.com/macros/s/AKfycbwvnyAISRYpsSrVg3Q_iQxo-0IE9O1ZQ2iene4haoByh6OMdxs4X6Y8S--dDjlUuxHr/exec

 *Web app URL Test Write : /https://script.google.com/macros/s/AKfycbwvnyAISRYpsSrVg3Q_iQxo-0IE9O1ZQ2iene4haoByh6OMdxs4X6Y8S--dDjlUuxHr/exec?sts=write&id=10&bc=10&srs=Success&temp=32.5&humd=95&Prs=989.55

 *Web app URL Test Read  : https://script.google.com/macros/s/AKfycbwvnyAISRYpsSrVg3Q_iQxo-0IE9O1ZQ2iene4haoByh6OMdxs4X6Y8S--dDjlUuxHr/exec?sts=read
**/

function doGet(e) {
  Logger.log(JSON.stringify(e));
  var result = 'OK';

  if (!e || !e.parameter || !e.parameter.id) {
    return ContentService.createTextOutput("❌ Missing required parameter 'id'");
  }

  var sheet_id = '1jLbKlstPxHlD7kHZhQn0Z-8PDUdkm7ZQxHZjDr8VWqw';  // Replace with your actual Sheet ID
  var unit_id = e.parameter.id;
  var sheet_name = "UNIT_" + unit_id;

  var sheet_open = SpreadsheetApp.openById(sheet_id);
  var sheet_target = sheet_open.getSheetByName(sheet_name);

  // Create sheet and headers if it doesn't exist
  if (!sheet_target) {
    sheet_target = sheet_open.insertSheet(sheet_name);
    sheet_target.getRange("A1:K1").setValues([[
      "Date", "Time", "ID", "Boot", "Battery", "Status", "Temp",
      "Humidity", "Pressure", "WindSpeed", "RFID"
    ]]);
  }

  var newRow = sheet_target.getLastRow() + 1;
  var rowData = new Array(11);

  // Date and Time in CST
  rowData[0] = Utilities.formatDate(new Date(), "CST", 'MM/dd/yyyy');
  rowData[1] = Utilities.formatDate(new Date(), "CST", 'HH:mm:ss');

  // Fill in parameters
  rowData[2] = e.parameter.id || "";
  rowData[3] = e.parameter.bc || "";
  rowData[4] = e.parameter.bat || "";
  rowData[5] = e.parameter.srs || "";
  rowData[6] = e.parameter.temp || "";
  rowData[7] = e.parameter.humd || "";
  rowData[8] = e.parameter.Prs || "";
  rowData[9] = e.parameter.wind || "";
  rowData[10] = e.parameter.rfid || "";

  Logger.log("Writing row: " + JSON.stringify(rowData));
  sheet_target.getRange(newRow, 1, 1, rowData.length).setValues([rowData]);

  return ContentService.createTextOutput(result);
}



/**
 * Utility to strip leading/trailing quotes from parameter values
 */
function stripQuotes(value) {
  return value.replace(/^['"]|['"]$/g, "");
}

