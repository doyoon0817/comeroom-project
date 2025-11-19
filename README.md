# comeroom-project

##전선연결
-1번 선: 1 (VCC)
-2번 선: 6 ( GND)
-3번 선: 24( nCS)
-4번 선: 23(CLK)
-5번 선: 21(MISO)
-6번 선: 19(MOSI)
-7번 선: 5(I2C_SCL)
-8번 선:3(I2C_SDA)
-9번 선: 11( reset)

##프로그램 시작 전 설정
'''sudo sh -c "echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor" '''
