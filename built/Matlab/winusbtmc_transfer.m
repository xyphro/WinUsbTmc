%winusbtmc_transfer Send a command to WinUsbTmc device and returns response
%
% This command sends a command like e.g. '*IDN?' to a devices.
% If the command es expected to return a response, set getresponse to true.
% It the command does not return a response, set getresponse to false.
%
% The parameter device can be either a device number (e.g. 0) or
% A device string identifier as returned by the function
% winusb_tmclist_devices
%
% Example:
% >> winusbtmc_transfer(0, '*IDN?', true)
% 
% ans =
% 
% *IDN SDG,SDG1020,SDG00004xxxxxx,1.01.01.23R1,02-00-00-19-24
%
function response = winusbtmc_transfer(device, command, getresponse)
    WINUSBTMC_EXECUTEABLEFILENAME = '..\winusbtmc.exe';
    
    if isnumeric(device)
       device = num2str(device); 
    end
        
    
    cmdin = [WINUSBTMC_EXECUTEABLEFILENAME ' '];
    if (getresponse == true)
        cmdin = [cmdin '/R'];
    end
    cmdin = [cmdin ' ' device ' ' command];
    
    [status, cmdout] = dos(cmdin);
    
    response = cmdout;
    
    
    