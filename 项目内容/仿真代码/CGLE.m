%======自相似孤子光纤激光器仿真（CGLE——1050nm）

clear;

%——————PBF中器件参数设定——————
angle_polarizing = pi/3 +pi/50;%PC起偏角
angle_PBS = angle_polarizing + pi/4;%PBS旋转角度
angle_add_phase = 0.9 * pi;%PC相移
alpha = 0.5;%PBF总能量损耗

%——————单模光纤、增益光纤、PBF参数设定——————
%单位——群速度色散：ps^2/m    非线性系数：(Wm)^(-1)   波数差：m^(-1)

%低双折射光纤参数
Bm = 1e-6;%双折射度10^(-6)
lambda_0 = 1050 * 1e-9;%脉冲中心波长1050nm
L_B = 1;%偏振拍长1m

%单模光纤1
beta_SMF1 = 0.025;%SMF1群速度色散
gamma_SMF1 = 0.0058;%SMF1非线性系数
L_SMF1 = 2.3;%SMF1长度
dbeta_SMF1 = 2 * pi * Bm / lambda_0;%SMF1双折射轴上波数差

%单模光纤2
beta_SMF2 = 0.025;%SMF2群速度色散
gamma_SMF2 = 0.0058;%SMF2非线性系数
L_SMF2 = 0.2;%SMF2长度
dbeta_SMF2 = 2 * pi * Bm / lambda_0;%SMF2双折射轴上波数差

%增益光纤
beta_YDF = 0.025;%YDF群速度色散
gamma_YDF = 0.0058;%YDF非线性系数
L_YDF = 0.3;%YDF长度
omega_g = 45e-9;%增益带宽45nm
g0 = 10;%小信号增益系数10/m
Esat = 4000;%增益饱和能量4000pJ
dbeta_YDF = 2 * pi * Bm / lambda_0;%YDF双折射轴上波数差

%PBF
L_PBF = 1.00;%PBF全长
beta_PBF = -0.05;%PBF群速度色散(模拟)

%谐振腔总色散(SMF1+SMF2+YDF+PBF)
DIS = beta_SMF1*L_SMF1 + beta_SMF2*L_SMF2 +beta_YDF*L_YDF +beta_PBF*L_PBF;
if DIS<0
    error('PBF长度设置有误，腔内色散为负');
end
%——————初始脉冲设定——————
%PS：此处t0非半高全宽表示 （t0=6ps —> FWHM=2.8841ps）

%脉冲参数
c = 3e8;%光速 3*10^8 m/s
t0 = 6;%脉冲时间参数c 单位：ps
time_windows = t0 * 10;%时间窗口
point_number = 2^12;%采样点数
delta_time = time_windows / point_number;%时间取样间隔
P0 = 1e-5;%初始功率0.01mW

%时域、频域范围
t = ((1:point_number)'-(point_number+1)/2) * delta_time;%时域
w = 2*pi * [(0:point_number/2-1),(-point_number/2:-1)]'/(time_windows);%频域fft

%脉冲形状
shape = 2;
switch shape
    case 1
        A0 = sqrt(P0) * sech(t/t0);%双曲正割
    case 2
        A0 = sqrt(P0) * exp(-0.5 * (t/t0).^2);%高斯
    case 3
        A0 = sqrt(P0) * (1 - (t/t0).^2);%抛物线
        for number_x1 = 1:point_number%取正
            if A0(number_x1)<=0
                A0(number_x1) = 0;
            end
        end
end
Uin = sin(angle_polarizing) * A0;%正交偏振分量Ax
Vin = sin(angle_polarizing) * A0;%正交偏振分量Ay

%——————循环及数据存入设定——————

roundtrip = 50;%循环次数
number_x2 = 1;%计数器——脉冲演化
Round_Trip = (1:roundtrip);%mesh图——X坐标数据矩阵(圈数1-100)
Aout = zeros(length(t),roundtrip);%脉冲输出矩阵
temp = zeros(length(t),roundtrip);%频谱输出矩阵
data_time = (zeros(roundtrip,length(t))).';%脉冲演化矩阵
data_temp = (zeros(roundtrip,length(t))).';%光谱演化矩阵
Apc = zeros(length(t),roundtrip);%脉冲PC输出矩阵

%循环步长设定
delta_L = 0.01;%光纤步长1cm
delta_L_P = 0.1;%PBF步长10cm
delta_L_SMF1 = L_SMF1 / delta_L;
delta_L_SMF2 = L_SMF2 / delta_L;
delta_L_YDF = L_YDF / delta_L;
delta_L_PBF = L_PBF / delta_L_P;

%——————谐振腔仿真——————

for number_x3 = 1:roundtrip
    
%SMF1
    f_u = fft(Uin);%Ax的傅里叶变换
    f_v = fft(Vin);%Ay的傅里叶变换
    D_u = (1i * 0.5 * dbeta_SMF1 + 1i * 0.5 * beta_SMF1 * w.^2);%作用于Ax的色散算子D
    D_v = (-1i * 0.5 * dbeta_SMF1 + 1i * 0.5 * beta_SMF1 * w.^2);%作用于Ay的色散算子D
    Duh = exp(D_u * 0.5 * delta_L);%完整的半步色散项1/2h*D_u
    Dvh = exp(D_v * 0.5 * delta_L);%完整的半步色散项1/2h*D_v
    
    for number_x4 = 1:delta_L_SMF1%从1到230循环
        
        %1/2色散作用
        if number_x4 == 1%脉冲入射后第一个半步色散
            u = ifft(Duh .* f_u);%频域处理色散
            v = ifft(Dvh .* f_v);%频域处理色散
        else
             D_u = (1i * 0.5 * dbeta_SMF1 + 1i * 0.5 * beta_SMF1 * w.^2);%作用于Ax的色散算子D
             D_v = (-1i * 0.5 * dbeta_SMF1 + 1i * 0.5 * beta_SMF1 * w.^2);%作用于Ay的色散算子D
             
             Duh = exp(D_u * 0.5 * delta_L);%完整的半步色散项1/2h*D_u
             Dvh = exp(D_v * 0.5 * delta_L);%完整的半步色散项1/2h*D_v
             
             f_u = fft(u_re);%完成上一次循环后的脉冲Ax再次傅里叶变换
             f_v = fft(v_re);%完成上一次循环后的脉冲Ay再次傅里叶变换
             
             u = ifft(Duh .* f_u);%Ax开始下一次半步色散作用
             v = ifft(Dvh .* f_v);%Ay开始下一次半步色散作用
        end
        
        %非线性作用——使用四阶龙格库塔算法
        u_cylinder = (u + 1i.*v)./sqrt(2);%圆柱坐标代换
        v_cylinder = (u - 1i.*v)./sqrt(2);%圆柱坐标代换
        
        k1 = 1i.*gamma_SMF1.*2./3.*((abs(u_cylinder)).^2 + 2.*(abs(v_cylinder)).^2).* u_cylinder;
        l1 = 1i.*gamma_SMF1.*2./3.*((abs(v_cylinder)).^2 + 2.*(abs(u_cylinder)).^2).* v_cylinder;
        u_cylinder1 = u_cylinder + delta_L * 0.5 * k1;
        v_cylinder1 = v_cylinder + delta_L * 0.5 * l1;
        k2 = 1i.*gamma_SMF1.*2./3.*((abs(u_cylinder1)).^2 + 2.*(abs(v_cylinder1)).^2).* u_cylinder1;
        l2 = 1i.*gamma_SMF1.*2./3.*((abs(v_cylinder1)).^2 + 2.*(abs(u_cylinder1)).^2).* v_cylinder1;
        u_cylinder2 = u_cylinder + delta_L * 0.5 * k2;
        v_cylinder2 = v_cylinder + delta_L * 0.5 * l2;
        k3 = 1i.*gamma_SMF1.*2./3.*((abs(u_cylinder2)).^2 + 2.*(abs(v_cylinder2)).^2).* u_cylinder2;
        l3 = 1i.*gamma_SMF1.*2./3.*((abs(v_cylinder2)).^2 + 2.*(abs(u_cylinder2)).^2).* v_cylinder2;
        u_cylinder3 = u_cylinder + delta_L * k3;
        v_cylinder3 = v_cylinder + delta_L * l3;
        k4 = 1i.*gamma_SMF1.*2./3.*((abs(u_cylinder3)).^2 + 2.*(abs(v_cylinder3)).^2).* u_cylinder3;
        l4 = 1i.*gamma_SMF1.*2./3.*((abs(v_cylinder3)).^2 + 2.*(abs(u_cylinder3)).^2).* v_cylinder3;
        u_cylinder_s = u_cylinder + (delta_L/6) * (k1+k2+k3+k4);
        v_cylinder_s = v_cylinder + (delta_L/6) * (l1+l2+l3+l4);
        
        u_n = (u_cylinder_s + v_cylinder_s)./sqrt(2);%直角坐标代换
        v_n = (u_cylinder_s - v_cylinder_s)./(sqrt(2).*1i);%直角坐标代换
        
        %1/2色散作用
        f_u = fft(u_n);
        f_v = fft(v_n);
        u = ifft(Duh .* f_u);
        v = ifft(Dvh .* f_v);
        u_re = u;
        v_re = v;
    end
    
    Uout1 = u;
    Vout1 = v;
% %     figure(1)
% %     subplot(1,3,1);
% %     plot(t,abs(cos(angle_PBS)*Uout1(:) + sin(angle_PBS)*Vout1(:)).^2,'-r');
% %     title('Uout--SMF1');
% %     xlabel('Time(ps)');
% %     ylabel('power(w)');
% %     xlim([-25 25]);
    
%YDF    
    k = 2*pi / lambda_0;%波数k
    % Tg = 2*pi / (c * k^2 * omega_g)*10^12; %增益模型
    Tg = 0.4; %增益模型
    
    f_u = fft(Uout1);
    f_v = fft(Vout1);
    
    Ep = trapz(t,abs(Uout1).^2 + abs(Vout1).^2);
    g = g0 / (1 + Ep/Esat);
    
    D_u = (1i*0.5*dbeta_YDF + 1i*0.5*beta_YDF*w.^2 + 0.5*g*(1-Tg^2*w.^2));
    D_v = (-1i*0.5*dbeta_YDF + 1i*0.5*beta_YDF*w.^2 + 0.5*g*(1-Tg^2*w.^2));
    
    Duh = exp(D_u * 0.5 * delta_L);
    Dvh = exp(D_v * 0.5 * delta_L);
    
    for number_x5 = 1:delta_L_YDF
        if number_x5 == 1
            u = ifft(Duh .* f_u);
            v = ifft(Dvh .* f_v);
        else
            Ep = trapz(t,abs(u_re).^2 + abs(v_re).^2);
            g = g0 / (1 + Ep/Esat);
            D_u = (1i*0.5*dbeta_YDF + 1i*0.5*beta_YDF*w.^2 + 0.5*g*(1-Tg^2*w.^2));
            D_v = (-1i*0.5*dbeta_YDF + 1i*0.5*beta_YDF*w.^2 + 0.5*g*(1-Tg^2*w.^2));
            Duh = exp(D_u * 0.5 * delta_L);
            Dvh = exp(D_v * 0.5 * delta_L);
            f_u = fft(u_re);
            f_v = fft(v_re);
            u = ifft(Duh .* f_u);
            v = ifft(Dvh .* f_v);
        end
        
        u_cylinder = (u + 1i.*v)./sqrt(2);
        v_cylinder = (u - 1i.*v)./sqrt(2);
        
        k1 = 1i.*gamma_YDF.*2./3.*((abs(u_cylinder)).^2 + 2.*(abs(v_cylinder)).^2).* u_cylinder;
        l1 = 1i.*gamma_YDF.*2./3.*((abs(v_cylinder)).^2 + 2.*(abs(u_cylinder)).^2).* v_cylinder;
        u_cylinder1 = u_cylinder + delta_L * 0.5 * k1;
        v_cylinder1 = v_cylinder + delta_L * 0.5 * l1;
        k2 = 1i.*gamma_YDF.*2./3.*((abs(u_cylinder1)).^2 + 2.*(abs(v_cylinder1)).^2).* u_cylinder1;
        l2 = 1i.*gamma_YDF.*2./3.*((abs(v_cylinder1)).^2 + 2.*(abs(u_cylinder1)).^2).* v_cylinder1;
        u_cylinder2 = u_cylinder + delta_L * 0.5 * k2;
        v_cylinder2 = v_cylinder + delta_L * 0.5 * l2;
        k3 = 1i.*gamma_YDF.*2./3.*((abs(u_cylinder2)).^2 + 2.*(abs(v_cylinder2)).^2).* u_cylinder2;
        l3 = 1i.*gamma_YDF.*2./3.*((abs(v_cylinder2)).^2 + 2.*(abs(u_cylinder2)).^2).* v_cylinder2;
        u_cylinder3 = u_cylinder + delta_L * k3;
        v_cylinder3 = v_cylinder + delta_L * l3;
        k4 = 1i.*gamma_YDF.*2./3.*((abs(u_cylinder3)).^2 + 2.*(abs(v_cylinder3)).^2).* u_cylinder3;
        l4 = 1i.*gamma_YDF.*2./3.*((abs(v_cylinder3)).^2 + 2.*(abs(u_cylinder3)).^2).* v_cylinder3;
        
        u_cylinder_s = u_cylinder + (delta_L/6) * (k1+k2+k3+k4);
        v_cylinder_s = v_cylinder + (delta_L/6) * (l1+l2+l3+l4); 
        
        u_n = (u_cylinder_s + v_cylinder_s)./sqrt(2);
        v_n = (u_cylinder_s - v_cylinder_s)./(sqrt(2).*1i);
        
        f_u = fft(u_n);
        f_v = fft(v_n);
        u = ifft(Duh .* f_u);
        v = ifft(Dvh .* f_v);
        u_re = u;
        v_re = v;
    end
    
    Uout2 = u;
    Vout2 = v;
% %     subplot(1,3,2);
% %     plot(t,abs(cos(angle_PBS)*Uout2(:) + sin(angle_PBS)*Vout2(:)).^2,'-r');
% %     title('Uout--YDF');
% %     xlabel('Time(ps)');
% %     ylabel('power(w)');
% %     xlim([-25 25]);
    
%SMF2
    u = fft(Uout2);
    v = fft(Vout2);
    D_u = (1i*0.5*dbeta_SMF2 + 1i*0.5*beta_SMF2*w.^2 + 0.5*g*(1-Tg^2*w.^2));
    D_v = (-1i*0.5*dbeta_SMF2 + 1i*0.5*beta_SMF2*w.^2 + 0.5*g*(1-Tg^2*w.^2));
    Duh = exp(D_u * 0.5 * delta_L);
    Dvh = exp(D_v * 0.5 * delta_L);
    
    for number_x6 = 1:delta_L_SMF2
        if number_x6 ==1
            u = ifft(Duh .* f_u);
            v = ifft(Dvh .* f_v);
        else
            D_u = (1i*0.5*dbeta_SMF2 + 1i*0.5*beta_SMF2*w.^2 + 0.5*g*(1-Tg^2*w.^2));
            D_v = (-1i*0.5*dbeta_SMF2 + 1i*0.5*beta_SMF2*w.^2 + 0.5*g*(1-Tg^2*w.^2));
            Duh = exp(D_u * 0.5 * delta_L);
            Dvh = exp(D_v * 0.5 * delta_L);
            f_u = fft(u_re);
            f_v = fft(v_re);
            u = ifft(Duh .* f_u);
            v = ifft(Dvh .* f_v);
        end
        
        u_cylinder = (u + 1i.*v)./sqrt(2);
        v_cylinder = (u - 1i.*v)./sqrt(2);
        
        k1 = 1i.*gamma_SMF2.*2./3.*((abs(u_cylinder)).^2 + 2.*(abs(v_cylinder)).^2).* u_cylinder;
        l1 = 1i.*gamma_SMF2.*2./3.*((abs(v_cylinder)).^2 + 2.*(abs(u_cylinder)).^2).* v_cylinder;
        u_cylinder1 = u_cylinder + delta_L * 0.5 * k1;
        v_cylinder1 = v_cylinder + delta_L * 0.5 * l1;
        k2 = 1i.*gamma_SMF2.*2./3.*((abs(u_cylinder1)).^2 + 2.*(abs(v_cylinder1)).^2).* u_cylinder1;
        l2 = 1i.*gamma_SMF2.*2./3.*((abs(v_cylinder1)).^2 + 2.*(abs(u_cylinder1)).^2).* v_cylinder1;
        u_cylinder2 = u_cylinder + delta_L * 0.5 * k2;
        v_cylinder2 = v_cylinder + delta_L * 0.5 * l2;
        k3 = 1i.*gamma_SMF2.*2./3.*((abs(u_cylinder2)).^2 + 2.*(abs(v_cylinder2)).^2).* u_cylinder2;
        l3 = 1i.*gamma_SMF2.*2./3.*((abs(v_cylinder2)).^2 + 2.*(abs(u_cylinder2)).^2).* v_cylinder2;
        u_cylinder3 = u_cylinder + delta_L * k3;
        v_cylinder3 = v_cylinder + delta_L * l3;
        k4 = 1i.*gamma_SMF2.*2./3.*((abs(u_cylinder3)).^2 + 2.*(abs(v_cylinder3)).^2).* u_cylinder3;
        l4 = 1i.*gamma_SMF2.*2./3.*((abs(v_cylinder3)).^2 + 2.*(abs(u_cylinder3)).^2).* v_cylinder3;
        u_cylinder_s = u_cylinder + (delta_L/6) * (k1+k2+k3+k4);
        v_cylinder_s = v_cylinder + (delta_L/6) * (l1+l2+l3+l4);
        
        u_n = (u_cylinder_s + v_cylinder_s)./sqrt(2);
        v_n = (u_cylinder_s - v_cylinder_s)./(sqrt(2).*1i);

        f_u = fft(u_n);
        f_v = fft(v_n);
        u = ifft(Duh .* f_u);
        v = ifft(Dvh .* f_v);
        u_re = u;
        v_re = v;
    end
    
    Uout3 = u;
    Vout3 = v;
% %     subplot(1,3,3);
% %     plot(t,abs(cos(angle_PBS)*Uout3(:) + sin(angle_PBS)*Vout3(:)).^2,'-r');
% %     title('Uout--SMF2');
% %     xlabel('Time(ps)');
% %     ylabel('power(w)');
% %     xlim([-25 25]);
    
%PC
   Uout4 = Uout3;
   Vout4 = Vout3;
   
   Vout4(:) = exp(1i * angle_add_phase) * Vout4(:);
   Apc(:,number_x3) = cos(angle_PBS)*Uout4(:) + sin(angle_PBS)*Vout4(:);
   
%PBF
   f_u = fft(Apc(:,number_x3));
   D_pbf = 1i * beta_PBF * w.^2;
   D_pbf_h = exp(D_pbf * 0.5 * delta_L_P);
   for number_x7 = 1:delta_L_PBF
       u = ifft(D_pbf_h .* f_u);
       v = ifft(D_pbf_h .* f_v);
   end
   Aout(:,number_x3) = u;
   Aout(:,number_x3) = Aout(:,number_x3) * sqrt(1-alpha);%加入50%腔内损耗
   Uin = sin(angle_polarizing) * Aout(:,number_x3);
   Vin = cos(angle_polarizing) * Aout(:,number_x3);
   temp(:,number_x3) = fftshift(ifft(Aout(:,number_x3))) .* time_windows / sqrt(2*pi);
   
%——————脉冲跟踪——————

%波形
figure(2);
subplot(3,3,1);
plot(t,abs(Aout(:,number_x3)).^2,'-k','linewidth',1.3);
title('波形');
xlabel('Time(ps)');
ylabel('Intensity(a.u.)');
xlim([-25 25]);
ylim([0 700]);

%光谱
subplot(3,3,2);
plot(c./(fftshift(w)./2./pi.*1e12+c./(lambda_0)).*1e9,abs(temp(:,number_x3)).^2,'-k','linewidth',1.3);
title('光谱');
xlabel('Wavelength(nm)');
ylabel('Intensity(a.u.)');
xlim([1030 1070]);

%光谱
% % figure(1);
% % plot(c./(fftshift(w)./2./pi.*1e12+c./(lambda_0)).*1e9,abs(temp(:,number_x3)).^2,'-k','linewidth',1.3);
% % title('光谱');
% % xlabel('Wavelength(nm)');
% % ylabel('Intensity(a.u.)');
% % xlim([1030 1070]);

%相位
subplot(3,3,4);
plot(t,phase(Aout(:,number_x3)),'--k');
title('相位');
xlabel('Time(ps)')
ylabel('Phase(rad)')
xlim([-25 25]);

%啁啾
subplot(3,3,5);
plot(t,-gradient(phase(Aout(:,number_x3)),t),'--k');
title('啁啾');
xlabel('Time(ps)')
ylabel('Chirp(THz)')
xlim([-25 25]);
ylim([-30 30]);

%正交分量
subplot(3,3,7);
plot(t,abs(Uin(:)).^2,'-r');
title('正交分量Ax');
xlabel('Time(ps)');
ylabel('power(w)');
xlim([-25 25]);
subplot(3,3,8);
plot(t,abs(Vin(:)).^2,'-r');
title('正交分量Ay');
xlabel('Time(ps)');
ylabel('power(w)');
xlim([-25 25]);

%演化
% % figure(3);
if(mod(roundtrip,1)==0)
        data_time(:,number_x2) = abs(Aout(:,number_x3)).^2;
        subplot(3,3,3);
        imagesc([1,number_x2],[1,2^12],data_time);
        title('脉冲时域演化');
        xlabel('RT');
        data_temp(:,number_x2) = abs(temp(:,number_x3)).^2;
        subplot(3,3,6);
        imagesc([1,number_x2],[1,2^12],data_temp);
        title('脉冲光谱演化');
        xlabel('RT');
        number_x2 = number_x2+1;
        colormap(hot);
end

end

%——————输出脉冲——————

%三维演化
% % figure(4);
subplot(3,3,9);
mesh(Round_Trip,t,abs(Aout).^2,'MeshStyle','col','EdgeColor','black');
set(gca,'YDir','reverse');
hidden off;
xlabel('Round Trips')
ylabel('Time(ps)')
zlabel('power(W)')
ylim([-25 25]);

%光谱演化图截取
number_cut = 10;%截取位置
figure(5);
data_temp2 = zeros(point_number,roundtrip);
data_temp2(:,[number_cut:roundtrip]) = data_temp(:,[number_cut:roundtrip]);
imagesc([1,roundtrip],[1,point_number],data_temp2);
colormap(hot);
title('脉冲光谱演化');
xlabel('RT');