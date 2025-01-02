<%@ Page Language="C#" AutoEventWireup="true" CodeFile="CISLatency.aspx.cs" Inherits="CISLatency" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="CISLatencyForm" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Number of minutes for CIS turnround in the past week" Width="640px"></asp:Label>
<br />
    <div>
    <asp:Chart ID="CISLatencyChart" runat="server" Height="1000px" Width="1500px" 
            SuppressExceptions="False">
            <Legends>
                <asp:Legend Name="Legend1" Docking="Bottom" Alignment="Center">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Legend="Legend1" 
                    Name="WaitTime" Color="Green" XValueType="DateTime">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Color="RoyalBlue" 
                    Legend="Legend1" Name="Duration" XValueType="DateTime">
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
