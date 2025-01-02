<%@ Page Language="C#" AutoEventWireup="true" CodeFile="RiftDVDSize.aspx.cs" Inherits="RiftDVDSize" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head id="Head1" runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="form1" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Rift DVD size (MB) vs. Date (Last 90 Days)" Width="720px"></asp:Label>
<br />
    <div>
    
        <asp:Chart ID="RiftDVDSizeChart" runat="server" Height="1000px" Width="1500px" 
			Palette="Excel">
            <Legends>
                <asp:Legend Name="Legend1" Alignment="Center" Docking="Bottom">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="GearDVDSize_INT" 
                    Legend="Legend1" Color="Blue" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="GearDVDSize_INT_FRA" 
                    Legend="Legend1" Color="Silver" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Color="DarkGray" 
					Legend="Legend1" Name="GearDVDSize_INT_ITA">
				</asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="GearDVDSize_INT_DEU" 
                    Legend="Legend1" Color="Gray" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Color="DimGray" 
					Legend="Legend1" Name="GearDVDSize_INT_ESN">
				</asp:Series>
				<asp:Series ChartArea="ChartArea1" ChartType="Line" Color="Silver" 
					Legend="Legend1" Name="GearDVDSize_INT_ESM">
				</asp:Series>
				<asp:Series ChartArea="ChartArea1" ChartType="Line" Color="Gray" 
					Legend="Legend1" Name="GearDVDSize_INT_JPN">
				</asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="GearDVDSize_WWSKU" 
                    Legend="Legend1" Color="Lime" >
                </asp:Series>  
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Xbox360DVDCapacity" 
                    Legend="Legend1" Color="Red" >
                </asp:Series>             
             </series>
            <chartareas>
                <asp:ChartArea Name="ChartArea1">
                </asp:ChartArea>
            </chartareas>
        </asp:Chart>
    
    </div>
    </form>
    </center>
</body>
</html>

