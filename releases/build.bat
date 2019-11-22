cd ..
del /s /q *.o
del /s /q *.d
cd monome-euro/multipass/monome_euro/ansible
make
cd ..\..\..\..\releases
copy ..\monome-euro\multipass\monome_euro\ansible\multipass_ans.hex orcas_heart_ans.hex

cd ..
del /s /q *.o
del /s /q *.d
cd monome-euro/multipass/monome_euro/earthsea
make
cd ..\..\..\..\releases
copy ..\monome-euro\multipass\monome_euro\earthsea\multipass_es.hex orcas_heart_es.hex

cd ..
del /s /q *.o
del /s /q *.d
cd monome-euro/multipass/monome_euro/meadowphysics
make
cd ..\..\..\..\releases
copy ..\monome-euro\multipass\monome_euro\meadowphysics\multipass_mp.hex orcas_heart_mp.hex

cd ..
del /s /q *.o
del /s /q *.d
cd monome-euro/multipass/monome_euro/teletype
make
cd ..\..\..\..\releases
copy ..\monome-euro\multipass\monome_euro\teletype\multipass_tt.hex orcas_heart_tt.hex

cd ..
del /s /q *.o
del /s /q *.d
cd monome-euro/multipass/monome_euro/whitewhale
make
cd ..\..\..\..\releases
copy ..\monome-euro\multipass\monome_euro\whitewhale\multipass_ww.hex orcas_heart_ww.hex
