
% Show a list of available usbtmc devices
winusbtmc_list_devices

% Send Identification command to the first device
idn_response = winusbtmc_transfer(0, '*IDN?', true)

% Alternative way to address a devices:
idn_response = winusbtmc_transfer('Rigol', '*IDN?', true)
