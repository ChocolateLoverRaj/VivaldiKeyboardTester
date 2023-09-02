$sourcePath = "./croskbsettings.bin"
$destinationPath = "C:\Windows\System32\drivers\croskbsettings.bin"

try {
    Copy-Item -Path $sourcePath -Destination $destinationPath -Force -ErrorAction Stop
    Write-Host "File copied successfully."
    Start-Process "C:\Program Files\crosec\croskbreload.exe"
}
catch {
    Write-Host "Error occurred while copying the file: $_"
    Write-Host "Try running this as administrator"
    # Wait for user input before closing the window
    Write-Host "Press any key to exit..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}
