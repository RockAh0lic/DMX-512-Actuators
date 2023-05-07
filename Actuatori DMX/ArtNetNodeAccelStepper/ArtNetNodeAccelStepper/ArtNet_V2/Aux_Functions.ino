void SelectChannel(int &canal) {
  lcd.setCursor(0, 0);
  lcd.print("Canal:");
  lcd.setCursor(0, 1);
  lcd.print(canal);
  int x = 1023;
  delay(1000);
  while (x < 600 || x > 800) {
    x = analogRead(0);
    if (x <= 60)
      canal += 10;
    else if (x > 60 && x <= 200)
      canal++;
    else if (x > 200 && x <= 400)
      canal--;
    else if (x > 400 && x <= 600)
      canal -= 10;
    if ( canal < 1 )
      canal = 1;
    if ( canal > 512 )
      canal = 512;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Canal:");
    lcd.setCursor(0, 1);
    lcd.print(canal);
    delay(100);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Channel set!");
  lcd.clear();
  lcd.print("Current channel:");
  lcd.setCursor(0, 1);
  lcd.print(canal);
}

bool check_new_command(int prev_params[6], int params[6]) {
  bool ok = false;
  for (int i = 0; i < 6; i++)
    if (prev_params[i] != params[i]) {
      prev_params[i] = params[i];
      ok = true;
    }
  return ok;
}
