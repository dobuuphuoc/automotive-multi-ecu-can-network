{ ===================================================================
  ECU_PlaceComponents.pas  -  Altium Designer (DelphiScript)
  Tu dat vi tri 73 linh kien cua board ECU Carrier theo bang toa do.

  Toa do: don vi 0.01 mm (3302 = 33.02 mm), goc (0,0) = goc DUOI-TRAI
          cua board outline. X sang phai, Y len tren.
  Huong : R = pin1 -> pin cuoi chay sang phai (+X)
          L = sang trai (-X)
          U = len tren (+Y)
          D = xuong duoi (-Y)
          M0 / M90 / M180 / M270 = dat Rotation truc tiep (dung cho U1, S1)

  Thu tu chay:
    1) TestJAIN       : chi dat J_AIN de kiem tra goc toa do va vi tri pin 1
    2) PlaceComponents: dat tat ca linh kien
  =================================================================== }

Var
  Items : TStringList;
  Warn  : String;

{ Doi don vi 0.01 mm sang don vi noi bo cua Altium (1 mm = 393700.787) }
Function Hmm2Coord(V : Integer) : Integer;
Begin
  Result := Round(V * 3937.00787);
End;

Procedure AddItems;
Begin
  Items := TStringList.Create;

  { Nhom 1: chuoi nguon (canh trai) }
  Items.Add('J_PWR;800;2000;D');
  Items.Add('D2;1600;2200;U');
  Items.Add('D1;1600;3000;U');
  Items.Add('F1;1600;3700;U');
  Items.Add('U_BUCK;1600;4700;U');
  Items.Add('C1;800;3600;U');
  Items.Add('C3;800;4700;U');
  Items.Add('TP_12V;2200;2600;R');
  Items.Add('TP_GND1;1200;600;R');

  { Nhom 2: goc trai-tren (5V, 3V3, den nguon) }
  Items.Add('JP_5V;2794;5850;R');
  Items.Add('C4;2400;5850;R');
  Items.Add('TP_5V;1800;6100;R');
  Items.Add('TP_3V3;2000;6600;R');
  Items.Add('TP_GND2;2000;7000;R');
  Items.Add('C_3V3_1;2500;6450;R');
  Items.Add('C_3V3_2;3000;6450;R');
  Items.Add('LED_PWR;3400;5900;R');
  Items.Add('R_PWR;3800;5900;R');

  { Nhom 3: khe Blue Pill }
  Items.Add('J_BP_L;7366;5334;L');
  Items.Add('J_BP_R;7366;3810;L');

  { Nhom 4: hang duoi }
  Items.Add('J_AIN;3302;762;R');
  Items.Add('TP_ADC;4400;508;R');
  Items.Add('R_ADC1;3556;1400;U');
  Items.Add('R_ADC2;3810;1400;U');
  Items.Add('C_ADC1;3556;2200;U');
  Items.Add('C_ADC2;3810;2200;U');
  Items.Add('D3;4300;1200;R');
  Items.Add('R2;4800;1200;R');
  Items.Add('S1;4500;1800;M0');
  Items.Add('R3;5150;1800;R');
  Items.Add('JP_ID0;4200;2400;R');
  Items.Add('JP_ID1;4800;2400;R');
  Items.Add('R4;4300;2700;R');
  Items.Add('R5;4900;2700;R');
  Items.Add('R6;5300;2400;R');
  Items.Add('C5;6858;2550;R');
  Items.Add('J_PWM1;5588;762;R');
  Items.Add('J_PWM2;6604;762;R');
  Items.Add('J_SPARE;7620;762;R');

  { Nhom 5: goc phai-duoi (dau ra so) }
  Items.Add('J_DOUT;9144;1270;U');
  Items.Add('R_PU1;7600;1270;R');
  Items.Add('R_PU2;7600;1524;R');
  Items.Add('R_PU3;7600;1778;R');
  Items.Add('R_PU4;7600;2032;R');
  Items.Add('JP_PU1;8128;1270;R');
  Items.Add('JP_PU2;8128;1524;R');
  Items.Add('JP_PU3;8128;1778;R');
  Items.Add('JP_PU4;8128;2032;R');

  { Nhom 6: goc phai-tren (dau vao so) }
  Items.Add('J_DIN;9144;5588;U');
  Items.Add('R_IN1;8500;5588;R');
  Items.Add('R_IN2;8500;5842;R');
  Items.Add('R_IN3;8500;6096;R');
  Items.Add('R_IN4;8500;6350;R');
  Items.Add('C_IN1;8000;5588;R');
  Items.Add('C_IN2;8000;5842;R');
  Items.Add('C_IN3;8000;6096;R');
  Items.Add('C_IN4;8000;6350;R');
  Items.Add('JP_DIN_V;8400;7000;R');

  { Nhom 7: hang connector canh tren }
  Items.Add('J_DHT;3048;7366;R');
  Items.Add('J_PWM4;4064;7366;R');
  Items.Add('J_PWM3;5080;7366;R');
  Items.Add('J_UART;6096;7366;R');
  Items.Add('J_ENC;7112;7366;R');
  Items.Add('JP_ENC_V;8128;7366;R');

  { Nhom 8: khoi CAN }
  Items.Add('U1;5200;6200;M0');
  Items.Add('C2;4600;6200;R');
  Items.Add('JP_RS;4400;6604;R');
  Items.Add('R1;6096;6000;R');
  Items.Add('JP_TERM;6500;6000;R');
  Items.Add('J_CAN1;6096;6604;R');
  Items.Add('J_CAN2;7112;6604;R');
  Items.Add('TP_CANH;7000;6000;R');
  Items.Add('TP_CANL;7500;6000;R');
End;

{ Tach 1 dong "REF;X;Y;DIR" }
Procedure ParseLine(Line : String; Var Ref : String; Var XX : Integer; Var YY : Integer; Var Dir : String);
Var
  P : Integer;
Begin
  P := Pos(';', Line);
  Ref := Copy(Line, 1, P - 1);
  Delete(Line, 1, P);

  P := Pos(';', Line);
  XX := StrToInt(Copy(Line, 1, P - 1));
  Delete(Line, 1, P);

  P := Pos(';', Line);
  YY := StrToInt(Copy(Line, 1, P - 1));
  Delete(Line, 1, P);

  Dir := Line;
End;

{ Tinh goc can xoay them de huong pin1 -> pin xa nhat khop voi Dir.
  Doc vi tri pad hien tai nen chay lai nhieu lan van cho ket qua dung. }
Function AutoDelta(Comp : IPCB_Component; Dir : String; RefName : String) : Integer;
Var
  Iter : IPCB_GroupIterator;
  Pad  : IPCB_Pad;
  Pad1 : IPCB_Pad;
  X1, Y1, FarX, FarY : Integer;
  dx, dy, d, Best : Double;
  Cur, Des : Integer;
Begin
  Result := 0;
  Pad1 := Nil;

  Iter := Comp.GroupIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(ePadObject));

  Pad := Iter.FirstPCBObject;
  While (Pad <> Nil) Do
  Begin
    If (Pad1 = Nil) And (Pad.Name = '1') Then Pad1 := Pad;
    Pad := Iter.NextPCBObject;
  End;

  If Pad1 = Nil Then
  Begin
    Comp.GroupIterator_Destroy(Iter);
    Warn := Warn + RefName + ' (khong co pad 1) ';
    Exit;
  End;

  X1 := Pad1.X;
  Y1 := Pad1.Y;
  Best := -1;
  FarX := X1;
  FarY := Y1;

  Pad := Iter.FirstPCBObject;
  While (Pad <> Nil) Do
  Begin
    dx := Pad.X - X1;
    dy := Pad.Y - Y1;
    d := dx * dx + dy * dy;
    If d > Best Then
    Begin
      Best := d;
      FarX := Pad.X;
      FarY := Pad.Y;
    End;
    Pad := Iter.NextPCBObject;
  End;
  Comp.GroupIterator_Destroy(Iter);

  dx := FarX - X1;
  dy := FarY - Y1;
  If Abs(dx) >= Abs(dy) Then
  Begin
    If dx >= 0 Then Cur := 0 Else Cur := 180;
  End
  Else
  Begin
    If dy >= 0 Then Cur := 90 Else Cur := 270;
  End;

  Des := 0;
  If Dir = 'R' Then Des := 0;
  If Dir = 'U' Then Des := 90;
  If Dir = 'L' Then Des := 180;
  If Dir = 'D' Then Des := 270;

  Result := (Des - Cur + 360) Mod 360;
End;

{ OnlyRef = '' : dat tat ca; nguoc lai chi dat linh kien co designator do }
Procedure DoPlace(OnlyRef : String);
Var
  Board : IPCB_Board;
  Comp  : IPCB_Component;
  R     : TCoordRect;
  AX, AY, I, XX, YY, Rot, Placed, W, H : Integer;
  Ref, Dir, Missing, Msg : String;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then
  Begin
    ShowMessage('Hay mo va chon tab file PCB (.PcbDoc) truoc khi chay script.');
    Exit;
  End;

  { Goc toa do = goc duoi-trai cua board outline }
  R := Board.BoardOutline.BoundingRectangle;
  AX := R.Left;
  AY := R.Bottom;
  W := Round((R.Right - R.Left) / 393700.787);
  H := Round((R.Top - R.Bottom) / 393700.787);

  If Not ConfirmNoYes('Board outline hien tai: ' + IntToStr(W) + ' x ' + IntToStr(H) +
                      ' mm. Goc duoi-trai se la (0,0). Phai la 100 x 80. Tiep tuc?') Then Exit;

  AddItems;
  Missing := '';
  Warn := '';
  Placed := 0;

  PCBServer.PreProcess;
  For I := 0 To Items.Count - 1 Do
  Begin
    ParseLine(Items[I], Ref, XX, YY, Dir);

    If (OnlyRef = '') Or (OnlyRef = Ref) Then
    Begin
      Comp := Board.GetPcbComponentByRefDes(Ref);
      If Comp = Nil Then
        Missing := Missing + Ref + ' '
      Else
      Begin
        If Copy(Dir, 1, 1) = 'M' Then
          Rot := StrToInt(Copy(Dir, 2, 5))
        Else
          Rot := (Round(Comp.Rotation) + AutoDelta(Comp, Dir, Ref)) Mod 360;

        PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
        Comp.Rotation := Rot;
        Comp.X := AX + Hmm2Coord(XX);
        Comp.Y := AY + Hmm2Coord(YY);
        PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
        Comp.GraphicallyInvalidate;
        Placed := Placed + 1;
      End;
    End;
  End;
  PCBServer.PostProcess;

  Board.ViewManager_FullUpdate;
  Client.SendMessage('PCB:Zoom', 'Action=All', 255, Client.CurrentView);

  Msg := 'Da dat ' + IntToStr(Placed) + ' linh kien.';
  If Missing <> '' Then Msg := Msg + Chr(13) + 'KHONG TIM THAY: ' + Missing;
  If Warn <> '' Then Msg := Msg + Chr(13) + 'CANH BAO: ' + Warn;
  ShowMessage(Msg);
End;

{ Chay thu: chi dat J_AIN tai (33.02, 7.62) mm }
Procedure TestJAIN;
Begin
  DoPlace('J_AIN');
End;

{ Chay that: dat toan bo linh kien }
Procedure PlaceComponents;
Begin
  DoPlace('');
End;
