%winusbtmc_list_devices Return a list of all present WinUsbTmc devices
%
% This function retuns a string showing all connected USBTMC devices
%
% Example: 
% >> winusbtmc_list_devices
% 
% ans =
% 
% 2 winusbtmc device(s) present.
% Device #0: "SIGLENT Technologies Co,. Ltd.:SDG1020:SDG0000xxxxxxx"
% Device #1: "Rigol Technologies:DS2202:DS2A1449xxxxx"
%
function response = winusbtmc_list_devices
    WINUSBTMC_EXECUTEABLEFILENAME = '..\winusbtmc.exe';
    
    [status, cmdout] = dos([WINUSBTMC_EXECUTEABLEFILENAME ' /L']);
    
    response = cmdout;
    
    
    