mkdir attrs
cd attrs
mkdir services

wget https://developer.bluetooth.org/gatt/services/Pages/ServicesHome.aspx

awk -vFPAT='org.bluetooth.service.[a-z_]*' '{
	for(i=1; i <= NF; i++)
		print $i

}' ServicesHome.aspx | sed -e's!.*!wget -O services/&.xml https://developer.bluetooth.org/gatt/services/Pages/ServiceViewer.aspx?u=&.xml!' | parallel 50
