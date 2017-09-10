

function body_onload() {
  document.getElementById('btnTipPreTax').onclick = function(){tipAmtTotal(0)};
  document.getElementById('btnTipOnTotal').onclick = function(){tipAmtTotal(1)};

}

function tipAmtTotal(flag) {

  //Checks if the inputs are valid
  if (tryParse(txtChkAmt.value, 2) == false ||
  parseFloat(txtChkAmt.value) < 0) {
    alert("Invalid input");
    document.getElementById('txtChkAmt').focus();
    return;
  }
  if (tryParse(txtTipsPrct.value, 2) == false ||
  parseFloat(txtTipsPrct.value) < 0) {
    alert("Invalid input");
    document.getElementById('txtTipsPrct').focus();
    return;
  }
  if (tryParse(txtSalesTaxPrct.value, 2) == false ||
  parseFloat(txtSalesTaxPrct.value) < 0) {
    alert("Invalid input");
    document.getElementById('txtSalesTaxPrct').focus();
    return;
  }

  var chkAmt = parseFloat(txtChkAmt.value);
  var tipsPrct = parseFloat(txtTipsPrct.value) / 100;
  var salesTax = parseFloat(txtSalesTaxPrct.value) / 100;

  var salesTaxTotal = chkAmt * salesTax;

  if (flag == 1) {
    chkAmt += salesTaxTotal;
  }

  var tipAmt = chkAmt * tipsPrct;

  txtSalesTaxAmt.value = salesTaxTotal.toFixed(2);
  txtTipAmt.value = tipAmt.toFixed(2);
}

function tryParse(str, pos) {

  //start of the while loop
  var len = str.length;
  var i = 0; //iterator
  var decimalCount = 0;

  //only checks if the the hyphen is at the beginning and move up one
  if(str.charAt(0) == '-'){
    i = 1;
  }

  //checks if any input isn't a number or decimal
  while (i < len) {

    //checks that there's only a single decimal
    if (str.charAt(i) == '.') {
      decimalCount++;
      if (decimalCount > pos) {
        return false;
      }

      //check if the length of decimal numbers is less than or equal to pos
      var lenOfDecimals = str.substring(i + 1).length;
      if (lenOfDecimals > pos) {
        return false;
      }

      i++;
      continue;
    }

    //checks if any character isn't a number and returns false
    if (isNaN(str.charAt(i))) {
      return false;
    }

    i++;
  }

  return true;
}
