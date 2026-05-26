#include <TFile.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TStyle.h>
#include <TString.h>
#include <TLatex.h>
#include <TROOT.h>
#include <iostream>
#include <vector>

// -------------------------------------------------------------------------
// Macro to merge Alignment Correlations and Analysis Residuals for MALTA 1 & 2
// -------------------------------------------------------------------------
void check_full_qc(TString align_filename, TString analysis_filename) {
    gStyle->SetOptStat(0);
    gStyle->SetOptFit(0);

    // Open both files simultaneously
    TFile *fileAlign = TFile::Open(align_filename);
    if (!fileAlign || fileAlign->IsZombie()) {
        std::cout << "[ERROR] Cannot open alignment file: " << align_filename << std::endl;
        return;
    }

    TFile *fileAnalysis = TFile::Open(analysis_filename);
    if (!fileAnalysis || fileAnalysis->IsZombie()) {
        std::cout << "[ERROR] Cannot open analysis file: " << analysis_filename << std::endl;
        return;
    }

    std::cout << "[INFO] Align File : " << align_filename << std::endl;
    std::cout << "[INFO] Analysis File: " << analysis_filename << std::endl;

    // Canvas configuration: 4 columns (CorrX, CorrY, ResX, ResY) x 2 rows (MALTA_1, MALTA_2)
    TCanvas *c1 = new TCanvas("c1", "Telescope Dual-File Hybrid QC", 1800, 750);
    c1->Divide(4, 2);

    // Focus only on moving detectors (MALTA_1 and MALTA_2)
    std::vector<TString> detectors = {"MALTA_1", "MALTA_2"};
    int padIndex = 1;
    
    for (size_t i = 0; i < detectors.size(); ++i) {
        TString det = detectors[i];

        // Define paths
        TString pathCorrX = Form("Correlations/%s/correlationX_2Dlocal", det.Data());
        TString pathCorrY = Form("Correlations/%s/correlationY_2Dlocal", det.Data());
        TString pathResX  = Form("Tracking4D/%s/local_residuals/LocalResidualsX", det.Data());
        TString pathResY  = Form("Tracking4D/%s/local_residuals/LocalResidualsY", det.Data());

        // --- 1. Draw Correlation X from Alignment File ---
        c1->cd(padIndex++);
        TH2F *hCorrX = (TH2F*)fileAlign->Get(pathCorrX);
        if (hCorrX) {
            hCorrX->SetTitle(Form("%s 2D Correlation X (Align Run)", det.Data()));
            hCorrX->SetDirectory(0); 
            hCorrX->Draw("colz");
        } else {
            std::cout << "[WARN] Missing in Align File: " << pathCorrX << std::endl;
        }

        // --- 2. Draw Correlation Y from Alignment File ---
        c1->cd(padIndex++);
        TH2F *hCorrY = (TH2F*)fileAlign->Get(pathCorrY);
        if (hCorrY) {
            hCorrY->SetTitle(Form("%s 2D Correlation Y (Align Run)", det.Data()));
            hCorrY->SetDirectory(0);
            hCorrY->Draw("colz");
        } else {
            std::cout << "[WARN] Missing in Align File: " << pathCorrY << std::endl;
        }

        // Helper lambda for Drawing Residuals from Analysis File
        auto drawResidual = [&](TString path, TString title) {
            int currentPad = padIndex++;
            c1->cd(currentPad);
            TH1F *hRes = (TH1F*)fileAnalysis->Get(path);
            if (hRes) {
                hRes->SetTitle(title);
                hRes->SetDirectory(0);
                hRes->SetLineColor(kBlue + 2);
                hRes->Draw();

                hRes->Fit("gaus", "Q");
                TF1 *fit = hRes->GetFunction("gaus");

                if (fit) {
                    double mean = fit->GetParameter(1);
                    double sigma = fit->GetParameter(2);

                    TLatex latex;
                    latex.SetNDC();
                    latex.SetTextFont(62);    // Helvetica Bold
                    latex.SetTextColor(kRed); 
                    latex.SetTextSize(0.06);

                    latex.DrawLatex(0.52, 0.80, Form("Mean: %.2f um", mean * 1000.0));
                    latex.DrawLatex(0.52, 0.72, Form("#sigma: %.2f um", sigma * 1000.0));
                }
            } else {
                std::cout << "[WARN] Missing in Analysis File: " << path << std::endl;
            }
        };

        // --- 3 & 4. Draw Residual X and Y from Analysis File ---
        drawResidual(pathResX, Form("%s Residual X (Analysis Run)", det.Data()));
        drawResidual(pathResY, Form("%s Residual Y (Analysis Run)", det.Data()));
    }

    c1->Modified();
    c1->Update();
    
    c1->SaveAs("alignment_qc_result.png");
    c1->SaveAs("alignment_qc_result.pdf");
    
    std::cout << "[DONE] Hybrid Dynamic QC Dashboard is completely rendered." << std::endl;
}