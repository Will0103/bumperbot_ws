#define L298N_enA 9
#define L298N_in1 12
#define L298N_in2 13

#define right_encoder_phaseA 3
#define right_encoder_phaseB 5

unsigned int right_encoder_counter = 0;
String right_encoder_sign = "p";
double right_wheel_meas_vel = 0.0;  // rad/s

void setup()
{
  pinMode(L298N_enA, OUTPUT);
  pinMode(L298N_in1, OUTPUT);
  pinMode(L298N_in2, OUTPUT);
  pinMode(right_encoder_phaseA, INPUT);
  pinMode(right_encoder_phaseB, INPUT);

  attachInterrupt(digitalPinToInterrupt(right_encoder_phaseA),rightEncoderCallback,RISING);

  digitalWrite(L298N_in1, HIGH);
  digitalWrite(L298N_in2, LOW);

  Serial.begin(115200);
}

void loop()
{
  // 每 100 ms 統計一次，所以 *10 轉成 pulse/sec
  // 11 pulse / motor rev
  // gearbox ratio = 35
  // 0.10472 = RPM -> rad/s
  right_wheel_meas_vel = 10.0 * right_encoder_counter * (60.0 / (11.0 * 35.0)) * 0.10472;
  String encoder_read = right_encoder_sign + String(right_wheel_meas_vel);

  Serial.println(encoder_read);
  analogWrite(L298N_enA, 200);
  right_encoder_counter = 0;
  delay(100);
}

void rightEncoderCallback()
{
  right_encoder_counter++;

  if (digitalRead(right_encoder_phaseB) == HIGH)
  {
    right_encoder_sign = "p";
  }
  else
  {
    right_encoder_sign = "n";
  }
}